/*   NAUTITECH - Simulateur de Navigation
     The weather window (METEO): presets, conditions, evolution and time of day.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "WeatherPanel.hpp"
#include "SimulationModel.hpp"
#include "GUIPanelDraw.hpp"
#include "launcher/HudText.hpp"

#include <cmath>
#include <cwchar>
#include <algorithm>

using station::Rect;

namespace
{
    const wchar_t* TAB_NAMES[6] = { L"Pr\u00E9r\u00E9glages", L"Vent & mer", L"Visibilit\u00E9 & pr\u00E9cip.", L"Courant", L"\u00C9volution", L"Heure & lumi\u00E8re" };
    const wchar_t* TAB_GROUPS[6] = { L"Conditions", L"Conditions", L"Conditions", L"Conditions", L"Temps", L"Temps" };
    const wchar_t* TAB_HELP[6] = {
        L"Un clic choisit un pr\u00E9r\u00E9glage, un double-clic ou la touche APPLIQUER le met en place : ciel, vent, mer, visibilit\u00E9 et pluie d'un coup. "
        L"La transition dit s'il s'installe tout de suite ou petit \u00E0 petit.",
        L"Le ciel, le vent et la mer. Chaque curseur agit tout de suite.",
        L"Ce qui r\u00E9duit la visibilit\u00E9 (brouillard, pluie, neige, sable) et les orages.",
        L"Le courant : celui de la mar\u00E9e de la zone, ou un courant impos\u00E9 dont vous choisissez la direction et la vitesse.",
        L"Comment le temps change pendant l'exercice : la dur\u00E9e du changement quand vous appliquez un pr\u00E9r\u00E9glage, et le passage d'une perturbation.",
        L"L'heure de la lumi\u00E8re : jour, aube, cr\u00E9puscule ou nuit." };
    const wchar_t* TRANSITION_NAMES[5] = { L"Imm\u00E9diate", L"1 min", L"5 min", L"15 min", L"30 min" };
    const irr::f32 TRANSITION_SECONDS[5] = { 0.0f, 60.0f, 300.0f, 900.0f, 1800.0f };
    const irr::f32 MOMENT_HOURS[6] = { 6.5f, 9.0f, 12.0f, 15.5f, 19.0f, 23.0f };

    //Fog slider: 0 % is 10 NM and more, 100 % is 50 m. Squared, so the low visibilities get most of the travel.
    irr::f32 fogToVisibility(irr::f32 fog)
    {
        fog = irr::core::clamp(fog, 0.0f, 1.0f);
        return 0.03f + 9.97f * powf(1.0f - fog, 2.2f);
    }
    irr::f32 visibilityToFog(irr::f32 nm)
    {
        if (nm >= 10.0f) { return 0.0f; }
        const irr::f32 x = irr::core::clamp((nm - 0.03f) / 9.97f, 0.0f, 1.0f);
        return 1.0f - powf(x, 1.0f / 2.2f);
    }

    //Visibility in nautical miles, or metres when under a mile
    void visibilityText(irr::f32 nm, std::wstring& value, std::wstring& unit)
    {
        wchar_t text[32];
        if (nm >= 1.0f) { swprintf(text, 32, L"%.1f", nm); unit = L"NM"; }
        else { swprintf(text, 32, L"%d", (int)(nm * 1852.0f / 10.0f + 0.5f) * 10); unit = L"m"; }
        value = text;
    }

    int beaufort(irr::f32 kn)
    {
        const irr::f32 limits[12] = { 1, 4, 7, 11, 17, 22, 28, 34, 41, 48, 56, 64 };
        int b = 0;
        while (b < 12 && kn >= limits[b]) { b++; }
        return b;
    }

    std::wstring clockOf(irr::f32 hours)
    {
        const int minutes = ((int)(hours * 60.0f + 0.5f)) % (24 * 60);
        wchar_t text[8];
        swprintf(text, 8, L"%02d:%02d", minutes / 60, minutes % 60);
        return text;
    }
}

//=================================================================================================

WeatherPanel::WeatherPanel(irr::gui::IGUIEnvironment* environment, irr::gui::IGUIElement* parent, SimulationModel* simulation,
    const irr::core::rect<irr::s32>& area)
    : IGUIElement(irr::gui::EGUIET_ELEMENT, environment, parent, -1, area),
    model(simulation), timeBox(0), tab(TAB_PRESETS), hoveredRow(-1), selectedRow(-1), draggedRow(-1), hoveredSeg(-1),
    hoveredPreset(-1), selectedPreset(0), chosenPreset(-1), hoveredKey(-1), transitionIndex(0)
{
    for (int i = 0; i < TAB_COUNT; i++) { scroll[i] = 0; }
    buildRows();
    buildPresets();
    frame.code = L"MTO 02";
    frame.title = L"Pupitre instructeur";
    frame.subtitle = L"M\u00E9t\u00E9o & environnement";
    for (int i = 0; i < TAB_COUNT; i++) {
        station::Frame::Page page;
        page.name = TAB_NAMES[i];
        page.group = TAB_GROUPS[i];
        frame.pages.push_back(page);
    }
    setVisible(false);
}

WeatherPanel::~WeatherPanel()
{
}

//-------------------------------------------------------------------------------------------------
//What is in it
//-------------------------------------------------------------------------------------------------

void WeatherPanel::buildRows()
{
    auto section = [](const wchar_t* text) {
        Row r; r.kind = ROW_SECTION; r.param = P_NONE; r.label = text; r.minValue = r.maxValue = r.step = 0;
        return r;
    };
    auto slider = [](Param p, const wchar_t* label, const wchar_t* title, const wchar_t* help, irr::f32 lo, irr::f32 hi, irr::f32 step) {
        Row r; r.kind = ROW_SLIDER; r.param = p; r.label = label; r.title = title; r.help = help;
        r.minValue = lo; r.maxValue = hi; r.step = step;
        return r;
    };
    auto choice = [](Param p, const wchar_t* label, const wchar_t* title, const wchar_t* help, std::vector<std::wstring> options) {
        Row r; r.kind = ROW_CHOICE; r.param = p; r.label = label; r.title = title; r.help = help;
        r.minValue = 0; r.maxValue = (irr::f32)options.size() - 1; r.step = 1; r.choices = options;
        return r;
    };

    std::vector<Row>& w = rows[TAB_WIND];
    w.push_back(section(L"Ciel"));
    w.push_back(slider(P_CLOUD, L"Couverture nuageuse", L"Couverture nuageuse",
        L"La quantit\u00E9 de nuages : 0 % = ciel bleu, 100 % = ciel tout gris. Plus il y a de nuages, plus il fait sombre.", 0, 100, 1));
    w.push_back(section(L"Vent"));
    w.push_back(slider(P_WIND, L"Vent moyen", L"Vent moyen",
        L"La force du vent, en n\u0153uds. Le vent pousse le navire et fait grossir les vagues.", 0, 70, 1));
    w.push_back(slider(P_WINDDIR, L"Direction du vent", L"Direction du vent",
        L"D'o\u00F9 vient le vent, en degr\u00E9s. Exemple : 000 = vent du nord, 090 = vent d'est.", 0, 359, 1));
    w.push_back(slider(P_WINDVAR, L"Variation de direction", L"Variation de direction",
        L"Le vent ne souffle jamais exactement de la m\u00EAme direction. Ce r\u00E9glage dit de combien de degr\u00E9s il peut tourner, \u00E0 gauche et \u00E0 droite.", 0, 90, 1));
    w.push_back(slider(P_GUST, L"Rafales", L"Rafales",
        L"Des coups de vent plus forts, par moments. Exemple : vent 15 n\u0153uds et rafales 10 n\u0153uds = des pointes \u00E0 25 n\u0153uds.", 0, 40, 1));
    w.push_back(section(L"Mer"));
    w.push_back(slider(P_SEA, L"\u00C9tat de la mer", L"\u00C9tat de la mer",
        L"La hauteur des vagues : 0 = mer plate, 6 = mer tr\u00E8s grosse.", 0, 6, 0.1f));

    std::vector<Row>& v = rows[TAB_VISIBILITY];
    v.push_back(section(L"Visibilit\u00E9 et pr\u00E9cipitations"));
    v.push_back(slider(P_FOG, L"Brouillard", L"Brouillard",
        L"Plus le chiffre est haut, moins on voit loin. 0 % : on voit \u00E0 plus de 10 milles. 100 % : on ne voit pas \u00E0 100 m\u00E8tres.", 0, 100, 1));
    v.push_back(slider(P_RAIN, L"Pluie", L"Pluie",
        L"La quantit\u00E9 de pluie, de la bruine \u00E0 la grosse averse. La pluie se voit aussi sur l'\u00E9cran radar.", 0, 100, 1));
    v.push_back(slider(P_SNOW, L"Neige", L"Neige",
        L"La quantit\u00E9 de neige. Plus il neige, moins on voit loin.", 0, 100, 1));
    v.push_back(slider(P_DUST, L"Sable / poussi\u00E8re", L"Sable et poussi\u00E8re",
        L"Du sable ou de la poussi\u00E8re dans l'air, couleur ocre. Plus le chiffre est haut, moins on voit loin.", 0, 100, 1));
    v.push_back(section(L"Orage et grains"));
    v.push_back(choice(P_SIGWX, L"Orage et grains", L"Orage et grains",
        L"Auto : l'orage arrive tout seul quand la mer devient grosse. Aucun : jamais d'orage. Orage : tonnerre et \u00E9clairs tout de suite. Grains : toutes les 5 minutes, un coup de vent avec une averse pendant 1 minute 30.",
        { L"Auto", L"Aucun", L"Orage", L"Grains" }));
    v.push_back(choice(P_THUNDER, L"Tonnerre", L"Tonnerre",
        L"Le bruit du tonnerre pendant l'orage. Non = orage sans bruit.", { L"Non", L"Oui" }));
    v.push_back(choice(P_LIGHTNING, L"\u00C9clairs", L"\u00C9clairs",
        L"Les \u00E9clairs dans le ciel pendant l'orage. Non = orage sans \u00E9clairs.", { L"Non", L"Oui" }));

    std::vector<Row>& c = rows[TAB_CURRENT];
    c.push_back(section(L"Courant"));
    c.push_back(choice(P_STREAM_MODE, L"Courant", L"Courant",
        L"Mar\u00E9e de la zone : le courant normal de la carte. Impos\u00E9 : vous choisissez vous-m\u00EAme la direction et la vitesse du courant, avec les r\u00E9glages ci-dessous.",
        { L"Mar\u00E9e de la zone", L"Impos\u00E9" }));
    c.push_back(slider(P_STREAM_DIR, L"Direction du courant", L"Direction du courant",
        L"Vers o\u00F9 le courant emporte le navire, en degr\u00E9s. Attention : c'est l'inverse du vent, qu'on donne d'o\u00F9 il vient.", 0, 359, 1));
    c.push_back(slider(P_STREAM_SPEED, L"Vitesse du courant", L"Vitesse du courant",
        L"La vitesse du courant, en n\u0153uds.", 0, 10, 0.1f));

    std::vector<Row>& e = rows[TAB_EVOLUTION];
    e.push_back(section(L"Pr\u00E9r\u00E9glages"));
    e.push_back(choice(P_TRANSITION, L"Dur\u00E9e de transition", L"Dur\u00E9e de transition",
        L"Quand vous appliquez un pr\u00E9r\u00E9glage, le temps change tout de suite (imm\u00E9diate), ou petit \u00E0 petit pendant la dur\u00E9e choisie.",
        { TRANSITION_NAMES[0], TRANSITION_NAMES[1], TRANSITION_NAMES[2], TRANSITION_NAMES[3], TRANSITION_NAMES[4] }));
    e.push_back(section(L"Perturbation"));
    e.push_back(choice(P_FRONT, L"Perturbation", L"Perturbation",
        L"Le mauvais temps arrive, puis repart tout seul. D'abord le vent forcit et tourne, la pluie arrive, la mer grossit et on voit moins loin. Ensuite tout redevient comme avant. Lente : environ 1 heure en tout. Rapide : environ 20 minutes.",
        { L"Aucune", L"Lente", L"Rapide" }));

    std::vector<Row>& t = rows[TAB_TIME];
    t.push_back(section(L"\u00C9clairage"));
    t.push_back(slider(P_HOUR, L"Heure", L"Heure de l'\u00E9clairage",
        L"L'heure de la lumi\u00E8re : jour, aube, cr\u00E9puscule ou nuit. Seule la lumi\u00E8re change : l'horloge, les autres navires et la mar\u00E9e de l'exercice ne bougent pas.", 0, 24, 0.25f));
    t.push_back(choice(P_MOMENT, L"Moment de la journ\u00E9e", L"Moment de la journ\u00E9e",
        L"Choix rapide de l'heure : aube, matin, midi, apr\u00E8s-midi, cr\u00E9puscule ou nuit.",
        { L"Aube", L"Matin", L"Midi", L"Apr\u00E8s-midi", L"Cr\u00E9puscule", L"Nuit" }));
}

void WeatherPanel::buildPresets()
{
    //name, help, cloud, wind, variation, gusts, visibility, rain, snow, dust, sea, significant
    auto add = [&](const wchar_t* name, const wchar_t* help, irr::f32 cloud, irr::f32 wind, irr::f32 variation, irr::f32 gusts,
        irr::f32 visibility, irr::f32 rain, irr::f32 snow, irr::f32 dust, irr::f32 sea, int significant) {
        Preset p;
        p.name = name; p.help = help; p.cloud = cloud; p.windKn = wind; p.windVariation = variation; p.gustKn = gusts;
        p.visibilityNm = visibility; p.rain = rain; p.snow = snow; p.dust = dust; p.sea = sea; p.significant = significant;
        presets.push_back(p);
    };
    add(L"Clair", L"Ciel d\u00E9gag\u00E9, petite brise, mer belle. Visibilit\u00E9 excellente.", 0.0f, 6, 10, 2, 12, 0, 0, 0, 0.5f, 1);
    add(L"Peu nuageux", L"Quelques nuages, brise l\u00E9g\u00E8re, mer peu agit\u00E9e.", 0.25f, 9, 12, 3, 10, 0, 0, 0, 1.0f, 1);
    add(L"Nuageux", L"Ciel nuageux, jolie brise, bonne visibilit\u00E9.", 0.55f, 13, 15, 5, 8, 0, 0, 0, 1.5f, 1);
    add(L"Couvert", L"Ciel couvert et gris, bonne brise, mer agit\u00E9e.", 0.9f, 16, 15, 6, 6, 0, 0, 0, 2.0f, 1);
    add(L"Pluie faible", L"Ciel couvert, bruine et pluie fine, visibilit\u00E9 moyenne.", 0.85f, 13, 15, 6, 4, 2.5f, 0, 0, 1.8f, 1);
    add(L"Pluie", L"Pluie soutenue, vent frais, visibilit\u00E9 r\u00E9duite.", 1.0f, 19, 20, 8, 2.5f, 6, 0, 0, 2.5f, 1);
    add(L"Brouillard", L"Brouillard \u00E9pais, vent faible, mer calme. Navigation au radar.", 0.7f, 3, 20, 0, 0.12f, 0, 0, 0, 0.5f, 1);
    add(L"Neige faible", L"Quelques flocons, ciel couvert, petit vent.", 0.85f, 8, 15, 4, 4, 0, 0.3f, 0, 1.2f, 1);
    add(L"Neige", L"Neige soutenue, visibilit\u00E9 r\u00E9duite.", 1.0f, 14, 20, 6, 2, 0, 0.75f, 0, 2.0f, 1);
    add(L"Orage", L"Orage : pluie forte, rafales, tonnerre et \u00E9clairs.", 1.0f, 26, 30, 15, 2, 8, 0, 0, 3.5f, 2);
    add(L"Blizzard", L"Temp\u00EAte de neige : vent fort, neige \u00E9paisse, visibilit\u00E9 presque nulle.", 1.0f, 40, 30, 15, 0.3f, 0, 1.0f, 0, 4.0f, 1);
    add(L"Sable / poussi\u00E8re", L"Brume de sable couleur ocre, on voit moins loin.", 0.3f, 16, 20, 6, 3, 0, 0, 0.45f, 1.5f, 1);
    add(L"Temp\u00EAte de sable", L"Vent fort charg\u00E9 de sable, visibilit\u00E9 tr\u00E8s r\u00E9duite.", 0.6f, 35, 30, 12, 0.3f, 0, 0, 1.0f, 3.0f, 1);
    add(L"Coup de vent", L"Coup de vent : mer forte, embruns, rafales.", 0.8f, 38, 20, 12, 5, 2, 0, 0, 4.5f, 1);
    add(L"Temp\u00EAte", L"Temp\u00EAte : mer tr\u00E8s grosse, pluie, orage, visibilit\u00E9 r\u00E9duite.", 1.0f, 55, 30, 18, 1.5f, 7, 0, 0, 6.0f, 2);
}

//-------------------------------------------------------------------------------------------------
//Values
//-------------------------------------------------------------------------------------------------

irr::f32 WeatherPanel::value(Param p) const
{
    if (!model) { return 0; }
    switch (p) {
    case P_CLOUD: return model->getCloudCover() * 100.0f;
    case P_WIND: return model->getWindSpeedBase();
    case P_WINDDIR: return model->getWindDirectionBase();
    case P_WINDVAR: return model->getWindVariation();
    case P_GUST: return model->getWindGust();
    case P_SEA: return model->getWeather();
    case P_FOG: return visibilityToFog(model->getVisibility()) * 100.0f;
    case P_RAIN: return model->getRain() * 10.0f;
    case P_SNOW: return model->getSnow() * 100.0f;
    case P_DUST: return model->getDust() * 100.0f;
    case P_HOUR: return model->getLightingTimeOfDay();
    case P_STREAM_DIR: return model->getStreamOverrideDirection();
    case P_STREAM_SPEED: return model->getStreamOverrideSpeed();
    default: return 0;
    }
}

void WeatherPanel::setValue(Param p, irr::f32 v)
{
    if (!model || model->getWeatherByInstructor()) { return; } //multiplayer: the instructor has the weather
    switch (p) {
    case P_CLOUD: model->setCloudCover(v / 100.0f); break;
    case P_WIND: model->setWindSpeed(v); break;
    case P_WINDDIR: model->setWindDirection(v); break;
    case P_WINDVAR: model->setWindVariation(v); break;
    case P_GUST: model->setWindGust(v); break;
    case P_SEA: model->setWeather(v); break;
    case P_FOG: model->setVisibility(fogToVisibility(v / 100.0f)); break;
    case P_RAIN: model->setRain(v / 10.0f); break;
    case P_SNOW: model->setSnow(v / 100.0f); break;
    case P_DUST: model->setDust(v / 100.0f); break;
    //Moving the current imposes it (the current is not part of a preset)
    case P_STREAM_DIR: model->setStreamOverrideDirection(v); model->setStreamOverride(true); return;
    case P_STREAM_SPEED: model->setStreamOverrideSpeed(v); model->setStreamOverride(true); return;
    case P_HOUR:
        model->setLightingTimeOfDay(v);
        if (timeBox) { timeBox->setText(clockOf(v).c_str()); }
        break;
    default: break;
    }
    if (p != P_HOUR) { chosenPreset = -1; } //set by hand now
}

int WeatherPanel::choiceIndex(Param p) const
{
    switch (p) {
    case P_SIGWX: return model ? model->getSignificantWeather() : 0;
    case P_TRANSITION: return transitionIndex;
    case P_FRONT: return model ? model->getWeatherFront() : 0;
    case P_THUNDER: return (model && model->getThunderEnabled()) ? 1 : 0;
    case P_LIGHTNING: return (model && model->getLightningEnabled()) ? 1 : 0;
    case P_STREAM_MODE: return (model && model->getStreamOverride()) ? 1 : 0;
    case P_MOMENT: {
        const irr::f32 h = value(P_HOUR);
        int best = 0;
        for (int i = 1; i < 6; i++) {
            if (fabsf(MOMENT_HOURS[i] - h) < fabsf(MOMENT_HOURS[best] - h)) { best = i; }
        }
        return best;
    }
    default: return 0;
    }
}

void WeatherPanel::setChoice(Param p, int index)
{
    if (model && model->getWeatherByInstructor() && p != P_TRANSITION) { return; }
    switch (p) {
    case P_SIGWX: if (model) { model->setSignificantWeather(index); } break;
    case P_TRANSITION: transitionIndex = irr::core::clamp(index, 0, 4); break;
    case P_FRONT: if (model) { model->setWeatherFront(index); } break;
    case P_THUNDER: if (model) { model->setThunderEnabled(index == 1); } break;
    case P_LIGHTNING: if (model) { model->setLightningEnabled(index == 1); } break;
    case P_STREAM_MODE: if (model) { model->setStreamOverride(index == 1); } break;
    case P_MOMENT: setValue(P_HOUR, MOMENT_HOURS[irr::core::clamp(index, 0, 5)]); break;
    default: break;
    }
}

std::wstring WeatherPanel::valueText(const Row& row) const
{
    wchar_t text[48];
    const irr::f32 v = value(row.param);
    switch (row.param) {
    case P_CLOUD: case P_FOG: case P_RAIN: case P_SNOW: case P_DUST:
        swprintf(text, 48, L"%d %%", (int)(v + 0.5f)); break;
    case P_WIND: case P_GUST:
        swprintf(text, 48, L"%d kn", (int)(v + 0.5f)); break;
    case P_WINDDIR: case P_STREAM_DIR:
        swprintf(text, 48, L"%03d\u00B0", ((int)(v + 0.5f)) % 360); break;
    case P_STREAM_SPEED:
        swprintf(text, 48, L"%.1f kn", v); break;
    case P_WINDVAR:
        swprintf(text, 48, L"\u00B1%d\u00B0", (int)(v + 0.5f)); break;
    case P_SEA:
        swprintf(text, 48, L"%.1f", v); break;
    case P_HOUR:
        return clockOf(v);
    default: text[0] = 0; break;
    }
    return text;
}

std::wstring WeatherPanel::stateText(const Row& row) const
{
    if (row.kind == ROW_SLIDER) { return valueText(row); }
    if (row.kind == ROW_CHOICE && !row.choices.empty()) {
        return row.choices[irr::core::clamp(choiceIndex(row.param), 0, (int)row.choices.size() - 1)];
    }
    return std::wstring();
}

irr::f32 WeatherPanel::transitionSeconds() const
{
    return TRANSITION_SECONDS[irr::core::clamp(transitionIndex, 0, 4)];
}

void WeatherPanel::applyPreset(int index)
{
    if (!model || index < 0 || index >= (int)presets.size() || model->getWeatherByInstructor()) { return; }
    const Preset& p = presets[index];
    SimulationModel::WeatherState s = model->getWeatherState();
    s.cloud = p.cloud;
    s.windKn = p.windKn;            //the direction stays where it is
    s.windVariation = p.windVariation;
    s.gustKn = p.gustKn;
    s.visibilityNm = p.visibilityNm;
    s.rain = p.rain;
    s.snow = p.snow;
    s.dust = p.dust;
    s.sea = p.sea;
    model->setWeatherState(s, transitionSeconds());
    model->setSignificantWeather(p.significant);
    chosenPreset = index;
    recordAction(L"Pr\u00E9r\u00E9glage", p.name + L" (" + TRANSITION_NAMES[irr::core::clamp(transitionIndex, 0, 4)] + L")");
}

void WeatherPanel::recordAction(const std::wstring& what, const std::wstring& v)
{
    lastAction = what + L" \u00BB " + v;
    if (model) {
        const int secs = (int)model->getExerciseLog().elapsedSeconds();
        wchar_t buf[32];
        swprintf(buf, 32, L"  \u00B7  T+ %02d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60);
        lastAction += buf;
    }
}

//-------------------------------------------------------------------------------------------------
//Layout
//-------------------------------------------------------------------------------------------------

void WeatherPanel::updateAbsolutePosition()
{
    IGUIElement::updateAbsolutePosition();
    if (AbsoluteRect != laidOutRect) { layout(); }
}

void WeatherPanel::layout()
{
    laidOutRect = AbsoluteRect;
    frame.layout(Environment, AbsoluteRect, 370, 330);
    clampScroll();
}

Rect WeatherPanel::content() const
{
    const irr::f32 k = frame.k;
    const irr::f32 pad = (frame.compact ? 10 : 16) * k;
    return Rect(frame.main.UpperLeftCorner.X + pad, frame.main.UpperLeftCorner.Y + 4 * k,
        frame.main.LowerRightCorner.X - pad, frame.main.LowerRightCorner.Y - 6 * k);
}

const std::vector<WeatherPanel::Row>& WeatherPanel::rowsOfTab() const
{
    return rows[tab];
}

irr::f32 WeatherPanel::rowHeight(const Row& row) const
{
    (void)row;
    return floorf(34 * frame.k);
}

Rect WeatherPanel::rowRect(int index) const
{
    const std::vector<Row>& rs = rowsOfTab();
    const Rect area = content();
    irr::f32 y = area.UpperLeftCorner.Y - scroll[tab];
    for (int i = 0; i < index && i < (int)rs.size(); i++) { y += rowHeight(rs[i]); }
    return Rect(area.UpperLeftCorner.X, y, area.LowerRightCorner.X - 8 * frame.k, y + floorf(34 * frame.k));
}

WeatherPanel::Cells WeatherPanel::cells(const Rect& r) const
{
    const irr::f32 k = frame.k;
    Cells c;
    const irr::f32 W = r.getWidth();
    const irr::f32 labelW = W * (frame.compact ? 0.38f : 0.30f);
    const irr::f32 stateW = frame.compact ? 0 : W * 0.21f;
    c.label = Rect(r.UpperLeftCorner.X + 12 * k, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + labelW - 6 * k, r.LowerRightCorner.Y);
    c.state = Rect(r.UpperLeftCorner.X + labelW, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + labelW + stateW, r.LowerRightCorner.Y);
    c.control = Rect(r.UpperLeftCorner.X + labelW + stateW, r.UpperLeftCorner.Y, r.LowerRightCorner.X - 4 * k, r.LowerRightCorner.Y);
    return c;
}

bool WeatherPanel::usesSegments(const Row& row, const Cells& c) const
{
    return row.kind == ROW_CHOICE && row.choices.size() >= 2 && row.choices.size() <= 6 &&
        station::segmentsFit(frame.fonts, frame.k, row.choices, c.control.getWidth());
}

std::vector<Rect> WeatherPanel::segmentRects(const Row& row, const Rect& r) const
{
    const Cells c = cells(r);
    if (!usesSegments(row, c)) { return std::vector<Rect>(); }
    return station::layoutSegments(frame.fonts, frame.k, row.choices, c.control, station::ALIGN_RIGHT, 24 * frame.k);
}

Rect WeatherPanel::stepperRect(const Cells& c) const
{
    const irr::f32 k = frame.k;
    const irr::f32 w = std::min(c.control.getWidth(), 300 * k);
    const irr::f32 cy = c.control.getCenter().Y;
    return Rect(floorf(c.control.LowerRightCorner.X - w), floorf(cy - 12 * k), floorf(c.control.LowerRightCorner.X), floorf(cy + 12 * k));
}

Rect WeatherPanel::sliderTrack(const Cells& c) const
{
    const irr::f32 k = frame.k;
    const irr::f32 x0 = c.control.UpperLeftCorner.X + (frame.compact ? 76 : 14) * k;
    const irr::f32 cy = c.control.getCenter().Y;
    return Rect(x0, cy - 2 * k, c.control.LowerRightCorner.X - 10 * k, cy + 2 * k);
}

int WeatherPanel::rowAt(const irr::core::vector2df& p) const
{
    if (tab == TAB_PRESETS || !content().isPointInside(p)) { return -1; }
    const std::vector<Row>& rs = rowsOfTab();
    for (int i = 0; i < (int)rs.size(); i++) {
        if (rs[i].kind == ROW_SECTION) { continue; }
        if (rowRect(i).isPointInside(p)) { return i; }
    }
    return -1;
}

int WeatherPanel::segmentAt(int row, const irr::core::vector2df& p) const
{
    const std::vector<Row>& rs = rowsOfTab();
    if (row < 0 || row >= (int)rs.size() || rs[row].kind != ROW_CHOICE) { return -1; }
    const Rect rr = rowRect(row);
    const std::vector<Rect> segs = segmentRects(rs[row], rr);
    for (int i = 0; i < (int)segs.size(); i++) {
        if (segs[i].isPointInside(p)) { return i; }
    }
    if (segs.empty()) {
        //The stepper: -2 its left arrow, -3 its right arrow
        const Rect s = stepperRect(cells(rr));
        if (s.isPointInside(p)) { return p.X < s.getCenter().X ? -2 : -3; }
    }
    return -1;
}

void WeatherPanel::dragSlider(int rowIndex, irr::s32 mouseX)
{
    const std::vector<Row>& rs = rowsOfTab();
    if (rowIndex < 0 || rowIndex >= (int)rs.size() || rs[rowIndex].kind != ROW_SLIDER) { return; }
    const Row& row = rs[rowIndex];
    const Rect track = sliderTrack(cells(rowRect(rowIndex)));
    irr::f32 t = ((irr::f32)mouseX - track.UpperLeftCorner.X) / std::max(1.0f, track.getWidth());
    t = irr::core::clamp(t, 0.0f, 1.0f);
    irr::f32 v = row.minValue + t * (row.maxValue - row.minValue);
    if (row.step > 0) { v = floorf(v / row.step + 0.5f) * row.step; }
    setValue(row.param, irr::core::clamp(v, row.minValue, row.maxValue));
}

//The preset page ----------------------------------------------------------------------------------

bool WeatherPanel::narrowTable() const
{
    return content().getWidth() < 600 * frame.k;
}

Rect WeatherPanel::tableHeaderRect() const
{
    const Rect area = content();
    const irr::f32 y = area.UpperLeftCorner.Y - scroll[TAB_PRESETS] + floorf(34 * frame.k);
    return Rect(area.UpperLeftCorner.X, y, area.LowerRightCorner.X - 8 * frame.k, y + floorf(26 * frame.k));
}

Rect WeatherPanel::presetRowRect(int index) const
{
    const Rect h = tableHeaderRect();
    const irr::f32 rh = floorf(30 * frame.k);
    const irr::f32 y = h.LowerRightCorner.Y + index * rh;
    return Rect(h.UpperLeftCorner.X, y, h.LowerRightCorner.X, y + rh);
}

std::vector<Rect> WeatherPanel::transitionRects() const
{
    const irr::f32 k = frame.k;
    const Rect last = presetRowRect((int)presets.size() - 1);
    const irr::f32 y = last.LowerRightCorner.Y + 14 * k;
    const irr::f32 labelW = frame.fonts.small ? frame.fonts.small->width(L"TRANSITION", 2.0f * k) + 14 * k : 80 * k;
    std::vector<std::wstring> texts(TRANSITION_NAMES, TRANSITION_NAMES + 5);
    return station::layoutSegments(frame.fonts, k, texts, Rect(last.UpperLeftCorner.X + labelW, y, last.LowerRightCorner.X, y + 28 * k),
        station::ALIGN_LEFT, 28 * k);
}

std::wstring WeatherPanel::applyCaption() const
{
    if (selectedPreset < 0 || selectedPreset >= (int)presets.size()) { return L"Appliquer"; }
    wchar_t num[8];
    swprintf(num, 8, L"%02d", selectedPreset + 1);
    return std::wstring(L"Appliquer ") + num + L" \u00B7 " + presets[selectedPreset].name;
}

Rect WeatherPanel::applyRect() const
{
    const irr::f32 k = frame.k;
    const std::vector<Rect> tr = transitionRects();
    const Rect last = presetRowRect((int)presets.size() - 1);
    const irr::f32 w = (frame.fonts.state ? frame.fonts.state->width(hudUpper(applyCaption()), 1.6f * k) : 200 * k) + 36 * k;
    const irr::f32 right = last.LowerRightCorner.X;
    if (!tr.empty() && tr.back().LowerRightCorner.X + 20 * k + w <= right) {
        return Rect(right - w, tr.back().UpperLeftCorner.Y, right, tr.back().LowerRightCorner.Y);
    }
    //Not enough room: on a line of its own
    const irr::f32 y = (tr.empty() ? last.LowerRightCorner.Y + 14 * k : tr.back().LowerRightCorner.Y) + 10 * k;
    return Rect(std::max(last.UpperLeftCorner.X, right - w), y, right, y + 28 * k);
}

int WeatherPanel::presetAt(const irr::core::vector2df& p) const
{
    if (tab != TAB_PRESETS || !content().isPointInside(p)) { return -1; }
    for (int i = 0; i < (int)presets.size(); i++) {
        if (presetRowRect(i).isPointInside(p)) { return i; }
    }
    return -1;
}

int WeatherPanel::presetKeyAt(const irr::core::vector2df& p) const
{
    if (tab != TAB_PRESETS || !content().isPointInside(p)) { return -1; }
    const std::vector<Rect> tr = transitionRects();
    for (int i = 0; i < (int)tr.size(); i++) { if (tr[i].isPointInside(p)) { return i; } }
    if (applyRect().isPointInside(p)) { return KEY_APPLY; }
    return -1;
}

irr::f32 WeatherPanel::contentHeight() const
{
    const Rect area = content();
    if (tab == TAB_PRESETS) {
        if (presets.empty()) { return 0; }
        return applyRect().LowerRightCorner.Y + 10 * frame.k + scroll[TAB_PRESETS] - area.UpperLeftCorner.Y;
    }
    irr::f32 h = 0;
    const std::vector<Row>& rs = rowsOfTab();
    for (size_t i = 0; i < rs.size(); i++) { h += rowHeight(rs[i]); }
    return h + 8 * frame.k;
}

void WeatherPanel::clampScroll()
{
    const irr::f32 maxScroll = std::max(0.0f, contentHeight() - content().getHeight());
    scroll[tab] = irr::core::clamp(scroll[tab], 0.0f, maxScroll);
}

//-------------------------------------------------------------------------------------------------
//Mouse
//-------------------------------------------------------------------------------------------------

bool WeatherPanel::OnEvent(const irr::SEvent& event)
{
    if (!IsVisible || !IsEnabled) { return IGUIElement::OnEvent(event); }
    if (event.EventType == irr::EET_GUI_EVENT) {
        if (event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_FOCUS_LOST && event.GUIEvent.Caller == this) { draggedRow = -1; }
        return IGUIElement::OnEvent(event);
    }
    if (event.EventType != irr::EET_MOUSE_INPUT_EVENT) { return IGUIElement::OnEvent(event); }

    const irr::SEvent::SMouseInput& m = event.MouseInput;
    const irr::core::position2di p(m.X, m.Y);
    const irr::core::vector2df pf((irr::f32)m.X, (irr::f32)m.Y);
    if (draggedRow < 0 && (dock.isDragging() || AbsoluteRect.isPointInside(p)) &&
        dock.onMouse(this, Environment, m, frame.header, frame.dockRect(dock.place), frame.closeRect())) {
        return true;
    }
    switch (m.Event) {
    case irr::EMIE_MOUSE_MOVED:
        if (draggedRow >= 0) { dragSlider(draggedRow, m.X); return true; }
        frame.hover(pf);
        hoveredRow = rowAt(pf);
        hoveredSeg = segmentAt(hoveredRow, pf);
        hoveredPreset = presetAt(pf);
        hoveredKey = presetKeyAt(pf);
        return AbsoluteRect.isPointInside(p);
    case irr::EMIE_LMOUSE_PRESSED_DOWN: {
        if (!AbsoluteRect.isPointInside(p)) { return false; }
        if (frame.closeRect().isPointInside(pf)) {
            setVisible(false);
            Environment->removeFocus(this);
            return true;
        }
        const int page = frame.pageAt(pf);
        if (page >= 0) {
            tab = page;
            selectedRow = hoveredRow = hoveredSeg = -1;
            clampScroll();
            return true;
        }
        if (tab == TAB_PRESETS) {
            const int key = presetKeyAt(pf);
            if (key == KEY_APPLY) { applyPreset(selectedPreset); return true; }
            if (key >= 0 && key < 5) {
                setChoice(P_TRANSITION, key);
                recordAction(L"Transition", TRANSITION_NAMES[key]);
                return true;
            }
            const int preset = presetAt(pf);
            if (preset >= 0) { selectedPreset = preset; }
            return true;
        }
        const int row = rowAt(pf);
        if (row >= 0) {
            selectedRow = row;
            const Row& r = rowsOfTab()[row];
            const Cells c = cells(rowRect(row));
            if (r.kind == ROW_SLIDER && c.control.isPointInside(pf)) {
                draggedRow = row;
                Environment->setFocus(this); //the moves come here while dragging, even off the row
                dragSlider(row, m.X);
            }
            else if (r.kind == ROW_CHOICE && !r.choices.empty()) {
                const int n = (int)r.choices.size();
                const int current = irr::core::clamp(choiceIndex(r.param), 0, n - 1);
                const int seg = segmentAt(row, pf);
                int next = current;
                if (seg >= 0) { next = seg; }
                else if (seg == -2) { next = (current + n - 1) % n; }
                else { next = (current + 1) % n; }  //right arrow, or the label
                if (next != current) {
                    setChoice(r.param, next);
                    recordAction(r.label, r.choices[next]);
                }
            }
        }
        return true;
    }
    case irr::EMIE_LMOUSE_DOUBLE_CLICK: {
        //A double click on a preset applies it
        const int preset = presetAt(pf);
        if (preset >= 0) {
            selectedPreset = preset;
            applyPreset(preset);
            return true;
        }
        return AbsoluteRect.isPointInside(p);
    }
    case irr::EMIE_LMOUSE_LEFT_UP:
        if (draggedRow >= 0) {
            const std::vector<Row>& rs = rowsOfTab();
            if (draggedRow < (int)rs.size()) { recordAction(rs[draggedRow].label, valueText(rs[draggedRow])); }
            draggedRow = -1;
            Environment->removeFocus(this); //keys go back to the ship
            return true;
        }
        return AbsoluteRect.isPointInside(p);
    case irr::EMIE_MOUSE_WHEEL:
        if (!AbsoluteRect.isPointInside(p)) { return false; }
        if (frame.main.isPointInside(pf)) {
            scroll[tab] -= m.Wheel * 64.0f * frame.k;
            clampScroll();
        }
        return true;
    default:
        return AbsoluteRect.isPointInside(p);
    }
}

//-------------------------------------------------------------------------------------------------
//Drawing
//-------------------------------------------------------------------------------------------------

void WeatherPanel::draw()
{
    if (!IsVisible) { return; }
    if (AbsoluteRect != laidOutRect) { layout(); }
    if (!frame.fonts.title) { return; }
    irr::video::IVideoDriver* driver = Environment->getVideoDriver();
    const irr::core::rect<irr::s32>& clip = AbsoluteClippingRect;

    //Header: what the weather is doing, the hour of the light
    std::vector<station::Status> head;
    if (model) {
        wchar_t text[64];
        if (model->getWeatherByInstructor()) {
            head.push_back({ L"M\u00E9t\u00E9o", L"Pilot\u00E9e par l'instructeur", station::TONE_WARN });
        }
        else if (model->getWeatherFront() != 0) {
            swprintf(text, 64, L"%d %%", (int)(model->getWeatherFrontProgress() * 100.0f + 0.5f));
            head.push_back({ L"Perturbation", text, station::TONE_WARN });
        }
        else if (model->isWeatherChanging()) {
            swprintf(text, 64, L"En cours %d %%", (int)(model->getWeatherChangeProgress() * 100.0f + 0.5f));
            head.push_back({ L"Transition", text, station::TONE_WARN });
        }
        else {
            head.push_back({ L"M\u00E9t\u00E9o", L"Stable", station::TONE_OK });
        }
        head.push_back({ L"Heure \u00E9clairage", clockOf(model->getLightingTimeOfDay()), station::TONE_NONE });
    }
    frame.drawBack(driver, dock, tab, head, clip);

    //The page (clipped, it scrolls)
    const Rect area = content();
    const irr::core::rect<irr::s32> pageClip = station::clipOf(Rect(frame.main.UpperLeftCorner.X, area.UpperLeftCorner.Y, frame.main.LowerRightCorner.X, area.LowerRightCorner.Y), clip);
    if (tab == TAB_PRESETS) { drawPresets(driver, pageClip); }
    else { drawRows(driver, pageClip); }
    station::drawScrollBar(driver, frame.k, Rect(area.UpperLeftCorner.X, area.UpperLeftCorner.Y, area.LowerRightCorner.X + 6 * frame.k, area.LowerRightCorner.Y),
        contentHeight(), scroll[tab], clip);

    drawSide(driver, clip);

    std::vector<station::Status> foot;
    foot.push_back({ frame.code, L"", 0 });
    foot.push_back({ L"Onglet", TAB_NAMES[tab], 0 });
    if (tab == TAB_PRESETS && selectedPreset >= 0 && selectedPreset < (int)presets.size()) {
        wchar_t num[8];
        swprintf(num, 8, L"%02d", selectedPreset + 1);
        foot.push_back({ L"S\u00E9lection", std::wstring(num) + L" \u00B7 " + presets[selectedPreset].name, 0 });
    }
    foot.push_back({ L"Derni\u00E8re action", lastAction.empty() ? std::wstring(L"\u2014") : lastAction, 0 });
    frame.drawFooter(driver, foot, tab == TAB_PRESETS ? L"Double-clic : appliquer  \u00B7  glisser le titre : d\u00E9placer"
                                                      : L"Molette : d\u00E9filer  \u00B7  glisser le titre : d\u00E9placer", clip);
}

void WeatherPanel::drawRows(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip)
{
    const station::Colours& col = station::colours();
    const station::Fonts& f = frame.fonts;
    const irr::f32 k = frame.k;
    const Rect area = content();
    const std::vector<Row>& rs = rowsOfTab();
    int odd = 0;
    for (int i = 0; i < (int)rs.size(); i++) {
        const Row& row = rs[i];
        const Rect r = rowRect(i);
        const bool visible = !(r.LowerRightCorner.Y < area.UpperLeftCorner.Y || r.UpperLeftCorner.Y > area.LowerRightCorner.Y);
        if (row.kind == ROW_SECTION) {
            odd = 0;
            if (!visible) { continue; }
            int count = 0;
            for (int j = i + 1; j < (int)rs.size() && rs[j].kind != ROW_SECTION; j++) { count++; }
            std::wstring items;
            if (count > 1) { items = std::to_wstring(count) + L" R\u00C9GLAGES"; }
            station::drawSection(driver, f, k, Rect(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y - 4 * k), row.label, items, clip);
            continue;
        }
        const bool shade = (odd++ % 2) == 0;
        if (!visible) { continue; }
        const bool active = (i == selectedRow) || (i == draggedRow);
        const bool hover = (i == hoveredRow);
        station::drawRowBack(driver, k, r, shade, hover, active, clip);
        const Cells c = cells(r);
        const irr::core::rect<irr::s32> lc = station::clipOf(c.label, clip);
        f.label->drawIn(row.label, Rect(c.label.UpperLeftCorner.X, c.label.UpperLeftCorner.Y, c.label.LowerRightCorner.X, c.label.LowerRightCorner.Y + 1 * k),
            col.text, HudFont::Left, 0.5f * k, &lc);
        const std::wstring state = stateText(row);
        if (c.state.getWidth() > 0) {
            const irr::core::rect<irr::s32> sc = station::clipOf(c.state, clip);
            f.state->drawIn(hudUpper(state), Rect(c.state.UpperLeftCorner.X, c.state.UpperLeftCorner.Y, c.state.LowerRightCorner.X - 6 * k, c.state.LowerRightCorner.Y + 1 * k),
                col.text, HudFont::Left, 1.0f * k, &sc);
        }
        if (row.kind == ROW_SLIDER) {
            const Rect track = sliderTrack(c);
            if (frame.compact) {
                f.state->drawIn(hudUpper(state), Rect(c.control.UpperLeftCorner.X, c.control.UpperLeftCorner.Y, track.UpperLeftCorner.X - 8 * k, c.control.LowerRightCorner.Y + 1 * k),
                    col.text, HudFont::Left, 1.0f * k, &clip);
            }
            const irr::f32 t = (value(row.param) - row.minValue) / std::max(0.001f, row.maxValue - row.minValue);
            station::drawFader(driver, k, track, t, active || hover, clip);
            continue;
        }
        const std::vector<Rect> segs = segmentRects(row, r);
        if (!segs.empty()) {
            const int on = choiceIndex(row.param);
            std::vector<station::Segment> ss(segs.size());
            for (size_t j = 0; j < segs.size(); j++) {
                ss[j].text = row.choices[j];
                ss[j].on = ((int)j == on);
                ss[j].hover = (i == hoveredRow && (int)j == hoveredSeg);
            }
            station::drawSegments(driver, f, k, segs, ss, clip);
        }
        else {
            const int side = (i == hoveredRow) ? (hoveredSeg == -2 ? -1 : (hoveredSeg == -3 ? 1 : 0)) : 0;
            station::drawStepper(driver, f, k, stepperRect(c), state, side, clip);
        }
    }
}

void WeatherPanel::drawPresets(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip)
{
    const station::Colours& col = station::colours();
    const station::Fonts& f = frame.fonts;
    const irr::f32 k = frame.k;
    const Rect head = tableHeaderRect();
    station::drawSection(driver, f, k, Rect(head.UpperLeftCorner.X, head.UpperLeftCorner.Y - 34 * k, head.LowerRightCorner.X, head.UpperLeftCorner.Y - 4 * k),
        L"Biblioth\u00E8que de pr\u00E9r\u00E9glages", std::to_wstring(presets.size()) + L" CONDITIONS", clip);

    //Columns: number, name, wind, gusts, visibility, sea, precipitation (narrow: no gusts, no sea)
    const bool narrow = narrowTable();
    std::vector<irr::f32> widths;
    std::vector<std::wstring> titles;
    if (narrow) {
        widths = { 0.09f, 0.31f, 0.15f, 0.17f, 0.28f };
        titles = { L"#", L"Pr\u00E9r\u00E9glage", L"Vent", L"Visibilit\u00E9", L"Pr\u00E9cipitations" };
    }
    else {
        widths = { 0.06f, 0.22f, 0.11f, 0.11f, 0.14f, 0.10f, 0.26f };
        titles = { L"#", L"Pr\u00E9r\u00E9glage", L"Vent", L"Rafales", L"Visibilit\u00E9", L"Mer", L"Pr\u00E9cipitations" };
    }
    std::vector<irr::f32> xs(widths.size() + 1);
    xs[0] = head.UpperLeftCorner.X;
    for (size_t i = 0; i < widths.size(); i++) { xs[i + 1] = xs[i] + widths[i] * head.getWidth(); }
    const irr::f32 tx = 10 * k;

    station::hline(driver, head.UpperLeftCorner.X, head.LowerRightCorner.X, head.LowerRightCorner.Y - 1, col.edge, &clip);
    for (size_t i = 0; i < titles.size(); i++) {
        const irr::core::rect<irr::s32> cc = station::clipOf(Rect(xs[i], head.UpperLeftCorner.Y, xs[i + 1], head.LowerRightCorner.Y), clip);
        f.small->drawIn(hudUpper(titles[i]), Rect(xs[i] + tx, head.UpperLeftCorner.Y, xs[i + 1], head.LowerRightCorner.Y), col.faint, HudFont::Left, 2.0f * k, &cc);
    }

    const Rect area = content();
    for (int i = 0; i < (int)presets.size(); i++) {
        const Rect r = presetRowRect(i);
        if (r.LowerRightCorner.Y < area.UpperLeftCorner.Y || r.UpperLeftCorner.Y > area.LowerRightCorner.Y) { continue; }
        const Preset& p = presets[i];
        const bool sel = (i == selectedPreset), hov = (i == hoveredPreset);
        if (sel) {
            station::fill(driver, r, col.selRow, &clip);
            station::frame(driver, r, col.accent, &clip);
            station::fill(driver, Rect(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + floorf(3 * k), r.LowerRightCorner.Y), col.accent, &clip);
        }
        else {
            station::drawRowBack(driver, k, r, (i % 2) == 0, hov, false, clip);
        }
        //Cells
        std::vector<std::wstring> cellText;
        wchar_t text[48];
        swprintf(text, 48, L"%02d", i + 1);
        cellText.push_back(text);
        cellText.push_back(hudUpper(p.name));
        swprintf(text, 48, L"%02d kn", (int)(p.windKn + 0.5f));
        cellText.push_back(text);
        if (!narrow) {
            if (p.gustKn >= 0.5f) { swprintf(text, 48, L"%02d kn", (int)(p.windKn + p.gustKn + 0.5f)); cellText.push_back(text); }
            else { cellText.push_back(L"\u2014"); }
        }
        std::wstring vis, unit;
        visibilityText(p.visibilityNm, vis, unit);
        cellText.push_back(vis + L" " + unit);
        if (!narrow) {
            swprintf(text, 48, L"%.1f", p.sea);
            cellText.push_back(text);
        }
        std::wstring precip;
        if (p.rain > 0) { swprintf(text, 48, L"Pluie %d %%", (int)(p.rain * 10.0f + 0.5f)); precip = text; }
        else if (p.snow > 0) { swprintf(text, 48, L"Neige %d %%", (int)(p.snow * 100.0f + 0.5f)); precip = text; }
        else if (p.dust > 0) { swprintf(text, 48, L"Sable %d %%", (int)(p.dust * 100.0f + 0.5f)); precip = text; }
        else { precip = L"Aucune"; }
        if (p.significant == 2) { precip += L" + orage"; }
        cellText.push_back(hudUpper(precip));

        const Rect rr(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y + 1 * k);
        for (size_t c = 0; c < cellText.size() && c + 1 < xs.size(); c++) {
            const irr::core::rect<irr::s32> cc = station::clipOf(Rect(xs[c], r.UpperLeftCorner.Y, xs[c + 1] - 4 * k, r.LowerRightCorner.Y), clip);
            const Rect box(xs[c] + tx, rr.UpperLeftCorner.Y, xs[c + 1], rr.LowerRightCorner.Y);
            if (c == 0) {
                f.navNum->drawIn(cellText[c], box, sel ? col.accentText : col.dim, HudFont::Left, 0.5f * k, &cc);
                //The preset in force: a lamp after its number
                if (i == chosenPreset) { station::lamp(driver, xs[c] + tx + 24 * k, r.getCenter().Y, 7 * k, col.ok, &cc); }
            }
            else if (c == 1) {
                f.state->drawIn(cellText[c], box, sel ? col.accentText : col.text, HudFont::Left, 1.0f * k, &cc);
            }
            else {
                f.key->drawIn(cellText[c], box, sel ? col.accentText : col.text.getInterpolated(col.dim, 0.75f), HudFont::Left, 0.5f * k, &cc);
            }
        }
    }

    //Transition keys and the apply key
    const std::vector<Rect> tr = transitionRects();
    if (!tr.empty()) {
        f.small->drawIn(L"TRANSITION", Rect(head.UpperLeftCorner.X, tr[0].UpperLeftCorner.Y, tr[0].UpperLeftCorner.X, tr[0].LowerRightCorner.Y + 1 * k),
            col.dim, HudFont::Left, 2.0f * k, &clip);
        std::vector<station::Segment> ss(tr.size());
        for (size_t j = 0; j < tr.size(); j++) {
            ss[j].text = TRANSITION_NAMES[j];
            ss[j].on = ((int)j == transitionIndex);
            ss[j].hover = (hoveredKey == (int)j);
        }
        station::drawSegments(driver, f, k, tr, ss, clip);
    }
    const bool locked = model && model->getWeatherByInstructor();
    station::drawKey(driver, f, k, applyRect(), applyCaption(), !locked, hoveredKey == KEY_APPLY, false, clip);
}

void WeatherPanel::drawSide(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip)
{
    const station::Fonts& f = frame.fonts;
    const irr::f32 k = frame.k;

    //What the explanation is about
    std::wstring title = TAB_NAMES[tab], help = TAB_HELP[tab];
    if (tab == TAB_PRESETS) {
        const int p = hoveredPreset >= 0 ? hoveredPreset : selectedPreset;
        if (hoveredKey >= 0 && hoveredKey < 5) {
            title = std::wstring(L"Transition \u2014 ") + TRANSITION_NAMES[hoveredKey];
            help = rows[TAB_EVOLUTION][1].help;
        }
        else if (p >= 0 && p < (int)presets.size()) {
            wchar_t num[8];
            swprintf(num, 8, L"%02d", p + 1);
            title = std::wstring(num) + L" \u00B7 " + presets[p].name;
            help = presets[p].help + (transitionIndex == 0 ? L" Appliqu\u00E9 imm\u00E9diatement." : L" Appliqu\u00E9 progressivement sur la dur\u00E9e de transition choisie.");
            if (p == chosenPreset) { help += L" C'est le pr\u00E9r\u00E9glage en place."; }
        }
    }
    else {
        const std::vector<Row>& rs = rowsOfTab();
        const int r = hoveredRow >= 0 ? hoveredRow : selectedRow;
        if (r >= 0 && r < (int)rs.size()) {
            title = rs[r].title;
            help = rs[r].help;
        }
    }
    if (model && model->getWeatherByInstructor()) {
        help += L" En multijoueur, la m\u00E9t\u00E9o est pilot\u00E9e depuis le poste instructeur : les r\u00E9glages sont bloqu\u00E9s ici.";
    }

    const irr::f32 pad = (frame.compact ? 10 : 14) * k;
    const Rect area(frame.side.UpperLeftCorner.X + pad, frame.side.UpperLeftCorner.Y + pad, frame.side.LowerRightCorner.X - pad, frame.side.LowerRightCorner.Y - pad);
    const irr::f32 ch = std::min(conditionsHeight(area.getWidth()), area.getHeight());
    const Rect cond(area.UpperLeftCorner.X, area.UpperLeftCorner.Y, area.LowerRightCorner.X, area.UpperLeftCorner.Y + ch);
    drawConditions(driver, cond, clip);
    Rect detail(area.UpperLeftCorner.X, cond.LowerRightCorner.Y + pad, area.LowerRightCorner.X, area.LowerRightCorner.Y);
    if (detail.getHeight() > 50 * k) {
        const irr::f32 need = station::boxHeaderHeight(k) + station::detailHeight(f, k, detail.getWidth(), title, help);
        if (need < detail.getHeight()) { detail.LowerRightCorner.Y = detail.UpperLeftCorner.Y + need; }
        const Rect in = station::drawBox(driver, f, k, detail, L"D\u00E9tail", clip);
        station::drawDetail(driver, f, k, in, title, help, clip);
    }
}

irr::f32 WeatherPanel::conditionsHeight(irr::f32 width) const
{
    const irr::f32 k = frame.k;
    const irr::f32 R = std::min(124 * k, width * 0.36f);
    const int colsBeside = irr::core::clamp((int)((width - R - 24 * k) / (86 * k)), 1, 4);
    const int rest = std::max(0, 10 - 2 * colsBeside);
    const int colsGrid = irr::core::clamp((int)(width / (140 * k)), 2, 5);
    const int rowsGrid = (rest + colsGrid - 1) / colsGrid;
    const bool changing = model && model->isWeatherChanging();
    return station::boxHeaderHeight(k) + 12 * k + R + 12 * k + rowsGrid * 54 * k + (changing ? 42 * k : 0) + 2;
}

void WeatherPanel::drawConditions(irr::video::IVideoDriver* driver, const Rect& box, const irr::core::rect<irr::s32>& clip)
{
    const station::Colours& col = station::colours();
    const station::Fonts& f = frame.fonts;
    const irr::f32 k = frame.k;
    const Rect in = station::drawBox(driver, f, k, box, L"Conditions actuelles", clip);
    if (!model) { return; }
    const irr::core::rect<irr::s32> ic = station::clipOf(in, clip);

    //The readings
    struct Reading { std::wstring caption, value, unit; };
    std::vector<Reading> rd;
    wchar_t text[48];
    swprintf(text, 48, L"%03d\u00B0", ((int)(model->getWindDirection() + 0.5f)) % 360);
    rd.push_back({ L"Vent", text, L"" });
    const irr::f32 wind = model->getWindSpeed();
    swprintf(text, 48, L"%d", (int)(wind + 0.5f));
    rd.push_back({ L"Force", text, L"kn" });
    const irr::f32 gust = model->getWindGust();
    if (gust >= 0.5f) { swprintf(text, 48, L"%d", (int)(model->getWindSpeedBase() + gust + 0.5f)); rd.push_back({ L"Rafales", text, L"kn" }); }
    else { rd.push_back({ L"Rafales", L"\u2014", L"" }); }
    rd.push_back({ L"Beaufort", std::to_wstring(beaufort(wind)), L"" });
    std::wstring vis, visUnit;
    visibilityText(model->getVisibility(), vis, visUnit);
    rd.push_back({ L"Visibilit\u00E9", vis, visUnit });
    swprintf(text, 48, L"%.1f", model->getWeather());
    rd.push_back({ L"\u00C9tat de mer", text, L"/ 6" });
    swprintf(text, 48, L"%d", (int)(model->getCloudCover() * 100.0f + 0.5f));
    rd.push_back({ L"Couverture", text, L"%" });
    if (model->getRain() > 0.05f) { swprintf(text, 48, L"%d", (int)(model->getRain() * 10.0f + 0.5f)); rd.push_back({ L"Pr\u00E9cip.", text, L"% pluie" }); }
    else if (model->getSnow() > 0.005f) { swprintf(text, 48, L"%d", (int)(model->getSnow() * 100.0f + 0.5f)); rd.push_back({ L"Pr\u00E9cip.", text, L"% neige" }); }
    else if (model->getDust() > 0.005f) { swprintf(text, 48, L"%d", (int)(model->getDust() * 100.0f + 0.5f)); rd.push_back({ L"Pr\u00E9cip.", text, L"% sable" }); }
    else { rd.push_back({ L"Pr\u00E9cip.", L"0", L"%" }); }
    if (model->getStreamOverride()) {
        swprintf(text, 48, L"%.1f", model->getStreamOverrideSpeed());
        wchar_t unit[32];
        swprintf(unit, 32, L"kn / %03d\u00B0", ((int)(model->getStreamOverrideDirection() + 0.5f)) % 360);
        rd.push_back({ L"Courant", text, unit });
    }
    else { rd.push_back({ L"Courant", L"MAR\u00C9E", L"" }); }
    rd.push_back({ L"Heure", clockOf(model->getLightingTimeOfDay()), L"\u00E9clairage" });

    auto cell = [&](const Rect& c, const Reading& r) {
        station::vline(driver, c.LowerRightCorner.X - 1, c.UpperLeftCorner.Y, c.LowerRightCorner.Y, col.rule, &ic);
        station::hline(driver, c.UpperLeftCorner.X, c.LowerRightCorner.X, c.LowerRightCorner.Y - 1, col.rule, &ic);
        const irr::core::rect<irr::s32> cc = station::clipOf(c, ic);
        f.small->drawIn(hudUpper(r.caption), Rect(c.UpperLeftCorner.X + 10 * k, c.UpperLeftCorner.Y + 6 * k, c.LowerRightCorner.X - 4 * k, c.UpperLeftCorner.Y + 20 * k),
            col.faint, HudFont::Left, 2.0f * k, &cc);
        //Value and unit on one baseline
        const irr::f32 top = c.UpperLeftCorner.Y + 24 * k + std::max(0.0f, (c.getHeight() - 24 * k - f.big->capHeight()) * 0.5f - 4 * k);
        const irr::f32 vw = f.big->draw(r.value, c.UpperLeftCorner.X + 10 * k, top, col.text, 0.5f * k, &cc);
        if (!r.unit.empty()) {
            f.unit->draw(r.unit, c.UpperLeftCorner.X + 10 * k + vw + 4 * k, top + f.big->capHeight() - f.unit->capHeight(), col.dim, 0.3f * k, &cc);
        }
    };

    const irr::f32 W = in.getWidth();
    const irr::f32 R = std::min(124 * k, W * 0.36f);
    const int colsBeside = irr::core::clamp((int)((W - R - 24 * k) / (86 * k)), 1, 4);
    const Rect rose(in.UpperLeftCorner.X + 12 * k, in.UpperLeftCorner.Y + 12 * k, in.UpperLeftCorner.X + 12 * k + R, in.UpperLeftCorner.Y + 12 * k + R);
    drawRose(driver, rose, model->getWindDirection(), ic);

    //Beside the rose: the wind
    const Rect beside(rose.LowerRightCorner.X + 12 * k, rose.UpperLeftCorner.Y, in.LowerRightCorner.X, rose.LowerRightCorner.Y);
    station::frame(driver, beside, col.rule, &ic);
    size_t n = 0;
    const irr::f32 bw = beside.getWidth() / colsBeside, bh = beside.getHeight() / 2;
    for (int row = 0; row < 2; row++) {
        for (int c = 0; c < colsBeside && n < rd.size(); c++, n++) {
            cell(Rect(beside.UpperLeftCorner.X + c * bw, beside.UpperLeftCorner.Y + row * bh, beside.UpperLeftCorner.X + (c + 1) * bw, beside.UpperLeftCorner.Y + (row + 1) * bh), rd[n]);
        }
    }

    //Under it: the rest, in a grid
    const int colsGrid = irr::core::clamp((int)(W / (140 * k)), 2, 5);
    irr::f32 y = rose.LowerRightCorner.Y + 12 * k;
    station::hline(driver, in.UpperLeftCorner.X, in.LowerRightCorner.X, y, col.rule, &ic);
    const irr::f32 gw = W / colsGrid, gh = 54 * k;
    for (int i = 0; n < rd.size(); i++, n++) {
        const int c = i % colsGrid, row = i / colsGrid;
        cell(Rect(in.UpperLeftCorner.X + c * gw, y + row * gh, in.UpperLeftCorner.X + (c + 1) * gw, y + (row + 1) * gh), rd[n]);
    }
    const int rest = std::max(0, 10 - 2 * colsBeside);
    y += ((rest + colsGrid - 1) / colsGrid) * gh;

    //A change under way: its progress
    if (model->isWeatherChanging()) {
        const irr::f32 prog = model->getWeatherChangeProgress();
        const bool front = model->getWeatherFront() != 0;
        const Rect line(in.UpperLeftCorner.X + 10 * k, y + 6 * k, in.LowerRightCorner.X - 10 * k, y + 22 * k);
        f.small->drawIn(front ? L"PERTURBATION EN COURS" : L"TRANSITION EN COURS", line, col.warn, HudFont::Left, 2.0f * k, &ic);
        swprintf(text, 48, L"%d %%", (int)(prog * 100.0f + 0.5f));
        f.state->drawIn(text, line, col.text, HudFont::Right, 1.0f * k, &ic);
        const Rect bar(line.UpperLeftCorner.X, y + 28 * k, line.LowerRightCorner.X, y + 32 * k);
        station::fill(driver, bar, col.edge2, &ic);
        station::fill(driver, Rect(bar.UpperLeftCorner.X, bar.UpperLeftCorner.Y, bar.UpperLeftCorner.X + bar.getWidth() * irr::core::clamp(prog, 0.0f, 1.0f), bar.LowerRightCorner.Y), col.warn, &ic);
    }
}

void WeatherPanel::drawRose(irr::video::IVideoDriver* driver, const Rect& r, irr::f32 from, const irr::core::rect<irr::s32>& clip)
{
    const station::Colours& col = station::colours();
    const station::Fonts& f = frame.fonts;
    const irr::f32 k = frame.k;
    const irr::core::vector2df c = r.getCenter();
    const irr::f32 rr = r.getWidth() * 0.5f;
    auto P = [&](irr::f32 deg, irr::f32 radius) {
        const irr::f32 a = deg * irr::core::DEGTORAD;
        return irr::core::vector2df(c.X + sinf(a) * radius, c.Y - cosf(a) * radius);
    };
    irr::gui::PanelBatch b;
    b.begin(driver);
    b.disc(c, rr, col.raised, col.raised);
    b.sector(c, rr - std::max(1.0f, 1.2f * k), rr, 0, 360, col.edge, col.edge);
    for (int a = 0; a < 360; a += 10) {
        const irr::f32 len = (a % 90 == 0) ? 9 * k : ((a % 30 == 0) ? 5 * k : 2.5f * k);
        b.line(P((irr::f32)a, rr - 2 * k - len), P((irr::f32)a, rr - 2 * k), std::max(1.0f, (a % 90 == 0 ? 1.5f : 1.0f) * k), a % 30 == 0 ? col.dim : col.faint);
    }
    //The wind: from where it comes, across the rose, the head downwind
    const irr::f32 to = from + 180.0f;
    const irr::core::vector2df tail = P(from, rr * 0.50f), tip = P(to, rr * 0.50f), base = P(to, rr * 0.50f - 12 * k);
    b.line(tail, base, std::max(1.5f, 2.6f * k), col.accent);
    const irr::f32 a = to * irr::core::DEGTORAD;
    const irr::core::vector2df side(cosf(a) * 6.5f * k, sinf(a) * 6.5f * k);
    b.tri(tip, base + side, base - side, col.accent);
    b.disc(tail, 3.5f * k, col.accent, col.accent);
    b.flush();
    const wchar_t* letters[4] = { L"N", L"E", L"S", L"W" };
    for (int i = 0; i < 4; i++) {
        const irr::core::vector2df q = P(i * 90.0f, rr - 20 * k);
        f.small->drawIn(letters[i], Rect(q.X - 8 * k, q.Y - 8 * k, q.X + 8 * k, q.Y + 8 * k), col.dim, HudFont::Centre, 0, &clip);
    }
}
