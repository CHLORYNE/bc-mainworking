/*   NAUTITECH - Simulateur de Navigation
     The weather window (METEO): presets, conditions, evolution and time of day.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "WeatherPanel.hpp"
#include "SimulationModel.hpp"
#include "BridgeSkin.hpp"
#include "GUIPanelDraw.hpp"
#include "launcher/HudText.hpp"

#include <cmath>
#include <cwchar>
#include <algorithm>

namespace
{
    //Pictures of the weather, drawn as shapes (they follow the palette and stay sharp at any size)
    enum WxIcon {
        WX_SUN = 0, WX_FEW, WX_CLOUDY, WX_OVERCAST, WX_RAIN_LIGHT, WX_RAIN, WX_FOG, WX_SNOW_LIGHT, WX_SNOW,
        WX_THUNDER, WX_BLIZZARD, WX_DUST, WX_DUSTSTORM, WX_GALE, WX_STORM, WX_WIND, WX_GUST, WX_SEA, WX_CLOCK,
        WX_FRONT, WX_COMPASS
    };

    const wchar_t* TAB_NAMES[4] = { L"PR\u00C9R\u00C9GLAGES", L"CONDITIONS", L"\u00C9VOLUTION", L"HEURE" };
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

    irr::video::SColor mix(irr::video::SColor a, irr::video::SColor b, irr::f32 t)
    {
        return b.getInterpolated(a, irr::core::clamp(t, 0.0f, 1.0f));
    }
    irr::video::SColor withAlpha(irr::video::SColor c, irr::u32 a)
    {
        c.setAlpha(a);
        return c;
    }
    irr::core::rect<irr::s32> toI(const irr::core::rect<irr::f32>& r)
    {
        return irr::core::rect<irr::s32>((irr::s32)floorf(r.UpperLeftCorner.X + 0.5f), (irr::s32)floorf(r.UpperLeftCorner.Y + 0.5f),
            (irr::s32)floorf(r.LowerRightCorner.X + 0.5f), (irr::s32)floorf(r.LowerRightCorner.Y + 0.5f));
    }
    //Rectangle clipped to another (draw2DRectangle takes a clip, PanelBatch does not)
    void fill(irr::video::IVideoDriver* d, const irr::core::rect<irr::f32>& r, irr::video::SColor c, const irr::core::rect<irr::s32>* clip)
    {
        d->draw2DRectangle(c, toI(r), clip);
    }
    //Hairline frame round a rectangle
    void outline(irr::video::IVideoDriver* d, const irr::core::rect<irr::s32>& r, irr::video::SColor c, const irr::core::rect<irr::s32>* clip)
    {
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.UpperLeftCorner.Y + 1), clip);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.LowerRightCorner.Y - 1, r.LowerRightCorner.X, r.LowerRightCorner.Y), clip);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + 1, r.LowerRightCorner.Y), clip);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.LowerRightCorner.X - 1, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y), clip);
    }
}

//=================================================================================================

WeatherPanel::WeatherPanel(irr::gui::IGUIEnvironment* environment, irr::gui::IGUIElement* parent, SimulationModel* simulation,
    const irr::core::rect<irr::s32>& area)
    : IGUIElement(irr::gui::EGUIET_ELEMENT, environment, parent, -1, area),
    model(simulation), timeBox(0), tab(TAB_PRESETS), hoveredRow(-1), selectedRow(-1), draggedRow(-1),
    hoveredPreset(-1), chosenPreset(-1), hoveredTab(-1), hoveredClose(false), transitionIndex(0), k(1.0f)
{
    for (int i = 0; i < TAB_COUNT; i++) { scroll[i] = 0; }
    buildRows();
    buildPresets();
    setVisible(false);
}

WeatherPanel::~WeatherPanel()
{
    freeFonts();
}

//-------------------------------------------------------------------------------------------------
//What is in it
//-------------------------------------------------------------------------------------------------

void WeatherPanel::buildRows()
{
    auto section = [](const wchar_t* text) {
        Row r; r.kind = ROW_SECTION; r.param = P_NONE; r.label = text; r.minValue = r.maxValue = r.step = 0; r.icon = -1;
        return r;
    };
    auto slider = [](Param p, const wchar_t* label, const wchar_t* title, const wchar_t* help, irr::f32 lo, irr::f32 hi, irr::f32 step, int icon) {
        Row r; r.kind = ROW_SLIDER; r.param = p; r.label = label; r.title = title; r.help = help;
        r.minValue = lo; r.maxValue = hi; r.step = step; r.icon = icon;
        return r;
    };
    auto choice = [](Param p, const wchar_t* label, const wchar_t* title, const wchar_t* help, std::vector<std::wstring> options, int icon) {
        Row r; r.kind = ROW_CHOICE; r.param = p; r.label = label; r.title = title; r.help = help;
        r.minValue = 0; r.maxValue = (irr::f32)options.size() - 1; r.step = 1; r.choices = options; r.icon = icon;
        return r;
    };

    std::vector<Row>& c = rows[TAB_CONDITIONS];
    c.push_back(section(L"CIEL"));
    c.push_back(slider(P_CLOUD, L"Couverture nuageuse [%]", L"Couverture nuageuse",
        L"La quantit\u00E9 de nuages : 0 % = ciel bleu, 100 % = ciel tout gris. Plus il y a de nuages, plus il fait sombre.", 0, 100, 1, WX_OVERCAST));
    c.push_back(section(L"VENT"));
    c.push_back(slider(P_WIND, L"Vent [n\u0153uds]", L"Vent moyen",
        L"La force du vent, en n\u0153uds. Le vent pousse le navire et fait grossir les vagues.", 0, 70, 1, WX_WIND));
    c.push_back(slider(P_WINDDIR, L"Direction du vent [\u00B0]", L"Direction du vent",
        L"D'o\u00F9 vient le vent, en degr\u00E9s. Exemple : 000 = vent du nord, 090 = vent d'est.", 0, 359, 1, WX_COMPASS));
    c.push_back(slider(P_WINDVAR, L"Variation de direction [\u00B0]", L"Variation de direction",
        L"Le vent ne souffle jamais exactement de la m\u00EAme direction. Ce r\u00E9glage dit de combien de degr\u00E9s il peut tourner, \u00E0 gauche et \u00E0 droite.", 0, 90, 1, WX_COMPASS));
    c.push_back(slider(P_GUST, L"Rafales [n\u0153uds]", L"Rafales",
        L"Des coups de vent plus forts, par moments. Exemple : vent 15 n\u0153uds et rafales 10 n\u0153uds = des pointes \u00E0 25 n\u0153uds.", 0, 40, 1, WX_GUST));
    c.push_back(section(L"MER"));
    c.push_back(slider(P_SEA, L"\u00C9tat de la mer", L"\u00C9tat de la mer",
        L"La hauteur des vagues : 0 = mer plate, 6 = mer tr\u00E8s grosse.", 0, 6, 0.1f, WX_SEA));
    c.push_back(section(L"COURANT"));
    c.push_back(choice(P_STREAM_MODE, L"Courant", L"Courant",
        L"Mar\u00E9e de la zone : le courant normal de la carte. Impos\u00E9 : vous choisissez vous-m\u00EAme la direction et la vitesse du courant, avec les r\u00E9glages ci-dessous.",
        { L"Mar\u00E9e de la zone", L"Impos\u00E9" }, WX_SEA));
    c.push_back(slider(P_STREAM_DIR, L"Direction du courant [\u00B0]", L"Direction du courant",
        L"Vers o\u00F9 le courant emporte le navire, en degr\u00E9s. Attention : c'est l'inverse du vent, qu'on donne d'o\u00F9 il vient.", 0, 359, 1, WX_COMPASS));
    c.push_back(slider(P_STREAM_SPEED, L"Vitesse du courant [n\u0153uds]", L"Vitesse du courant",
        L"La vitesse du courant, en n\u0153uds.", 0, 10, 0.1f, WX_SEA));
    c.push_back(section(L"VISIBILIT\u00C9 ET PR\u00C9CIPITATIONS"));
    c.push_back(slider(P_FOG, L"Brouillard [%]", L"Brouillard",
        L"Plus le chiffre est haut, moins on voit loin. 0 % : on voit \u00E0 plus de 10 milles. 100 % : on ne voit pas \u00E0 100 m\u00E8tres.", 0, 100, 1, WX_FOG));
    c.push_back(slider(P_RAIN, L"Pluie [%]", L"Pluie",
        L"La quantit\u00E9 de pluie, de la bruine \u00E0 la grosse averse. La pluie se voit aussi sur l'\u00E9cran radar.", 0, 100, 1, WX_RAIN));
    c.push_back(slider(P_SNOW, L"Neige [%]", L"Neige",
        L"La quantit\u00E9 de neige. Plus il neige, moins on voit loin.", 0, 100, 1, WX_SNOW));
    c.push_back(slider(P_DUST, L"Sable / poussi\u00E8re [%]", L"Sable et poussi\u00E8re",
        L"Du sable ou de la poussi\u00E8re dans l'air, couleur ocre. Plus le chiffre est haut, moins on voit loin.", 0, 100, 1, WX_DUST));
    c.push_back(section(L"ORAGE ET GRAINS"));
    c.push_back(choice(P_SIGWX, L"Orage et grains", L"Orage et grains",
        L"Auto : l'orage arrive tout seul quand la mer devient grosse. Aucun : jamais d'orage. Orage : tonnerre et \u00E9clairs tout de suite. Grains : toutes les 5 minutes, un coup de vent avec une averse pendant 1 minute 30.",
        { L"Auto", L"Aucun", L"Orage", L"Grains" }, WX_THUNDER));
    c.push_back(choice(P_THUNDER, L"Tonnerre", L"Tonnerre",
        L"Le bruit du tonnerre pendant l'orage. Non = orage sans bruit.", { L"Non", L"Oui" }, WX_THUNDER));
    c.push_back(choice(P_LIGHTNING, L"\u00C9clairs", L"\u00C9clairs",
        L"Les \u00E9clairs dans le ciel pendant l'orage. Non = orage sans \u00E9clairs.", { L"Non", L"Oui" }, WX_THUNDER));

    std::vector<Row>& e = rows[TAB_EVOLUTION];
    e.push_back(section(L"PR\u00C9R\u00C9GLAGES"));
    e.push_back(choice(P_TRANSITION, L"Dur\u00E9e du changement de temps", L"Dur\u00E9e du changement",
        L"Quand vous choisissez un pr\u00E9r\u00E9glage, le temps change tout de suite (Imm\u00E9diat), ou petit \u00E0 petit pendant la dur\u00E9e choisie.",
        { L"Imm\u00E9diat", L"1 minute", L"5 minutes", L"15 minutes", L"30 minutes" }, WX_CLOCK));
    e.push_back(section(L"PERTURBATION"));
    e.push_back(choice(P_FRONT, L"Perturbation", L"Perturbation",
        L"Le mauvais temps arrive, puis repart tout seul. D'abord le vent forcit et tourne, la pluie arrive, la mer grossit et on voit moins loin. Ensuite tout redevient comme avant. Lente : environ 1 heure en tout. Rapide : environ 20 minutes.",
        { L"Aucune", L"Lente", L"Rapide" }, WX_FRONT));

    std::vector<Row>& t = rows[TAB_TIME];
    t.push_back(section(L"\u00C9CLAIRAGE"));
    t.push_back(slider(P_HOUR, L"Heure [h]", L"Heure de l'\u00E9clairage",
        L"L'heure de la lumi\u00E8re : jour, aube, cr\u00E9puscule ou nuit. Seule la lumi\u00E8re change : l'horloge, les autres navires et la mar\u00E9e de l'exercice ne bougent pas.", 0, 24, 0.25f, WX_CLOCK));
    t.push_back(choice(P_MOMENT, L"Moment de la journ\u00E9e", L"Moment de la journ\u00E9e",
        L"Choix rapide de l'heure : aube, matin, midi, apr\u00E8s-midi, cr\u00E9puscule ou nuit.",
        { L"Aube", L"Matin", L"Midi", L"Apr\u00E8s-midi", L"Cr\u00E9puscule", L"Nuit" }, WX_SUN));
}

void WeatherPanel::buildPresets()
{
    //name, help, icon, cloud, wind, variation, gusts, visibility, rain, snow, dust, sea, significant
    auto add = [&](const wchar_t* name, const wchar_t* help, int icon, irr::f32 cloud, irr::f32 wind, irr::f32 variation, irr::f32 gusts,
        irr::f32 visibility, irr::f32 rain, irr::f32 snow, irr::f32 dust, irr::f32 sea, int significant) {
        Preset p;
        p.name = name; p.help = help; p.icon = icon; p.cloud = cloud; p.windKn = wind; p.windVariation = variation; p.gustKn = gusts;
        p.visibilityNm = visibility; p.rain = rain; p.snow = snow; p.dust = dust; p.sea = sea; p.significant = significant;
        presets.push_back(p);
    };
    add(L"Clair", L"Ciel d\u00E9gag\u00E9, petite brise, mer belle. Visibilit\u00E9 excellente.", WX_SUN, 0.0f, 6, 10, 2, 12, 0, 0, 0, 0.5f, 1);
    add(L"Peu nuageux", L"Quelques nuages, brise l\u00E9g\u00E8re, mer peu agit\u00E9e.", WX_FEW, 0.25f, 9, 12, 3, 10, 0, 0, 0, 1.0f, 1);
    add(L"Nuageux", L"Ciel nuageux, jolie brise, bonne visibilit\u00E9.", WX_CLOUDY, 0.55f, 13, 15, 5, 8, 0, 0, 0, 1.5f, 1);
    add(L"Couvert", L"Ciel couvert et gris, bonne brise, mer agit\u00E9e.", WX_OVERCAST, 0.9f, 16, 15, 6, 6, 0, 0, 0, 2.0f, 1);
    add(L"Pluie faible", L"Ciel couvert, bruine et pluie fine, visibilit\u00E9 moyenne.", WX_RAIN_LIGHT, 0.85f, 13, 15, 6, 4, 2.5f, 0, 0, 1.8f, 1);
    add(L"Pluie", L"Pluie soutenue, vent frais, visibilit\u00E9 r\u00E9duite.", WX_RAIN, 1.0f, 19, 20, 8, 2.5f, 6, 0, 0, 2.5f, 1);
    add(L"Brouillard", L"Brouillard \u00E9pais, vent faible, mer calme. Navigation au radar.", WX_FOG, 0.7f, 3, 20, 0, 0.12f, 0, 0, 0, 0.5f, 1);
    add(L"Neige faible", L"Quelques flocons, ciel couvert, petit vent.", WX_SNOW_LIGHT, 0.85f, 8, 15, 4, 4, 0, 0.3f, 0, 1.2f, 1);
    add(L"Neige", L"Neige soutenue, visibilit\u00E9 r\u00E9duite.", WX_SNOW, 1.0f, 14, 20, 6, 2, 0, 0.75f, 0, 2.0f, 1);
    add(L"Orage", L"Orage : pluie forte, rafales, tonnerre et \u00E9clairs.", WX_THUNDER, 1.0f, 26, 30, 15, 2, 8, 0, 0, 3.5f, 2);
    add(L"Blizzard", L"Temp\u00EAte de neige : vent fort, neige \u00E9paisse, visibilit\u00E9 presque nulle.", WX_BLIZZARD, 1.0f, 40, 30, 15, 0.3f, 0, 1.0f, 0, 4.0f, 1);
    add(L"Sable / poussi\u00E8re", L"Brume de sable couleur ocre, on voit moins loin.", WX_DUST, 0.3f, 16, 20, 6, 3, 0, 0, 0.45f, 1.5f, 1);
    add(L"Temp\u00EAte de sable", L"Vent fort charg\u00E9 de sable, visibilit\u00E9 tr\u00E8s r\u00E9duite.", WX_DUSTSTORM, 0.6f, 35, 30, 12, 0.3f, 0, 0, 1.0f, 3.0f, 1);
    add(L"Coup de vent", L"Coup de vent : mer forte, embruns, rafales.", WX_GALE, 0.8f, 38, 20, 12, 5, 2, 0, 0, 4.5f, 1);
    add(L"Temp\u00EAte", L"Temp\u00EAte : mer tr\u00E8s grosse, pluie, orage, visibilit\u00E9 r\u00E9duite.", WX_STORM, 1.0f, 55, 30, 18, 1.5f, 7, 0, 0, 6.0f, 2);
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
        if (timeBox) {
            const int minutes = ((int)(v * 60.0f + 0.5f)) % (24 * 60);
            wchar_t text[8];
            swprintf(text, 8, L"%02d:%02d", minutes / 60, minutes % 60);
            timeBox->setText(text);
        }
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
    case P_HOUR: {
        const int minutes = ((int)(v * 60.0f + 0.5f)) % (24 * 60);
        swprintf(text, 48, L"%02d:%02d", minutes / 60, minutes % 60);
        break;
    }
    default: text[0] = 0; break;
    }
    return text;
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
}

//-------------------------------------------------------------------------------------------------
//Layout
//-------------------------------------------------------------------------------------------------

void WeatherPanel::loadFonts()
{
    freeFonts();
    irr::video::IVideoDriver* driver = Environment->getVideoDriver();
    irr::gui::IGUIFont* fallback = Environment->getSkin() ? Environment->getSkin()->getFont() : 0;
    const std::string folder = "media/fonts/barlow-condensed/";
    fonts.title = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 34 * k, fallback);
    fonts.tab = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 20 * k, fallback);
    fonts.label = new HudFont(driver, folder + "BarlowCondensed-Regular.ttf", 21 * k, fallback);
    fonts.value = new HudFont(driver, folder + "BarlowCondensed-Medium.ttf", 20 * k, fallback);
    fonts.section = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 16 * k, fallback);
    fonts.infoTitle = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 30 * k, fallback);
    fonts.infoText = new HudFont(driver, folder + "BarlowCondensed-Regular.ttf", 21 * k, fallback);
    fonts.caption = new HudFont(driver, folder + "BarlowCondensed-SemiBold.ttf", 18 * k, fallback);
}

void WeatherPanel::freeFonts()
{
    HudFont** all[] = { &fonts.title, &fonts.tab, &fonts.label, &fonts.value, &fonts.section, &fonts.infoTitle, &fonts.infoText, &fonts.caption };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        delete *all[i];
        *all[i] = 0;
    }
}

void WeatherPanel::updateAbsolutePosition()
{
    IGUIElement::updateAbsolutePosition();
    if (AbsoluteRect != laidOutRect) { layout(); }
}

void WeatherPanel::layout()
{
    laidOutRect = AbsoluteRect;
    const irr::core::rect<irr::f32> r((irr::f32)AbsoluteRect.UpperLeftCorner.X, (irr::f32)AbsoluteRect.UpperLeftCorner.Y,
        (irr::f32)AbsoluteRect.LowerRightCorner.X, (irr::f32)AbsoluteRect.LowerRightCorner.Y);
    const irr::f32 W = r.getWidth(), H = r.getHeight();
    if (W < 10 || H < 10) { return; }
    const irr::f32 newK = irr::core::clamp(std::min(H / 720.0f, W / 1200.0f), 0.6f, 2.0f);
    if (fabsf(newK - k) > 0.01f || !fonts.title) {
        k = newK;
        loadFonts();
    }
    const irr::f32 pad = 22 * k;
    header = irr::core::rect<irr::f32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.UpperLeftCorner.Y + 62 * k);
    tabsBar = irr::core::rect<irr::f32>(r.UpperLeftCorner.X + pad, header.LowerRightCorner.Y, r.LowerRightCorner.X - pad, header.LowerRightCorner.Y + 40 * k);
    const irr::f32 bodyTop = tabsBar.LowerRightCorner.Y + 18 * k;
    const irr::f32 bodyBottom = r.LowerRightCorner.Y - pad;
    const irr::f32 listW = (W - 3 * pad) * 0.6f;
    list = irr::core::rect<irr::f32>(r.UpperLeftCorner.X + pad, bodyTop, r.UpperLeftCorner.X + pad + listW, bodyBottom);
    info = irr::core::rect<irr::f32>(list.LowerRightCorner.X + pad, bodyTop, r.LowerRightCorner.X - pad, bodyBottom);
    const irr::f32 tileSide = std::min(info.getWidth() * 0.62f, info.getHeight() * 0.42f);
    const irr::f32 tileTop = info.UpperLeftCorner.Y + std::min(170 * k, info.getHeight() * 0.36f);
    tile = irr::core::rect<irr::f32>(info.UpperLeftCorner.X, tileTop, info.UpperLeftCorner.X + tileSide, tileTop + tileSide);
    for (int i = 0; i < TAB_COUNT; i++) { clampScroll(); }
}

irr::core::rect<irr::f32> WeatherPanel::tabRect(int t) const
{
    const irr::f32 w = 168 * k, gap = 6 * k;
    const irr::f32 total = TAB_COUNT * w + (TAB_COUNT - 1) * gap;
    const irr::f32 x0 = (tabsBar.UpperLeftCorner.X + tabsBar.LowerRightCorner.X) * 0.5f - total * 0.5f + t * (w + gap);
    return irr::core::rect<irr::f32>(x0, tabsBar.UpperLeftCorner.Y, x0 + w, tabsBar.LowerRightCorner.Y);
}

irr::core::rect<irr::f32> WeatherPanel::closeRect() const
{
    const irr::f32 s = 34 * k;
    const irr::f32 x1 = header.LowerRightCorner.X - 16 * k, y0 = header.UpperLeftCorner.Y + (header.getHeight() - s) * 0.5f;
    return irr::core::rect<irr::f32>(x1 - s, y0, x1, y0 + s);
}

const std::vector<WeatherPanel::Row>& WeatherPanel::rowsOfTab() const
{
    return rows[tab];
}

irr::core::rect<irr::f32> WeatherPanel::rowRect(int index) const
{
    const std::vector<Row>& rs = rowsOfTab();
    irr::f32 y = list.UpperLeftCorner.Y - scroll[tab];
    for (int i = 0; i < index && i < (int)rs.size(); i++) {
        y += (rs[i].kind == ROW_SECTION ? 30 * k : 40 * k) + 4 * k;
    }
    const irr::f32 h = (index < (int)rs.size() && rs[index].kind == ROW_SECTION) ? 30 * k : 40 * k;
    return irr::core::rect<irr::f32>(list.UpperLeftCorner.X, y, list.LowerRightCorner.X - 14 * k, y + h);
}

irr::core::rect<irr::f32> WeatherPanel::sliderTrack(const irr::core::rect<irr::f32>& row) const
{
    const irr::f32 cellX = row.UpperLeftCorner.X + row.getWidth() * 0.56f + 4 * k;
    const irr::f32 x0 = cellX + 78 * k, x1 = row.LowerRightCorner.X - 18 * k;
    const irr::f32 cy = row.getCenter().Y;
    return irr::core::rect<irr::f32>(x0, cy - 3 * k, x1, cy + 3 * k);
}

irr::core::rect<irr::f32> WeatherPanel::presetRect(int index) const
{
    const int columns = list.getWidth() > 620 * k ? 4 : 3;
    const irr::f32 gap = 14 * k;
    const irr::f32 w = (list.getWidth() - 14 * k - (columns - 1) * gap) / columns;
    const irr::f32 h = 30 * k + w * 0.78f;
    const int col = index % columns, row = index / columns;
    const irr::f32 x = list.UpperLeftCorner.X + col * (w + gap);
    const irr::f32 y = list.UpperLeftCorner.Y - scroll[TAB_PRESETS] + row * (h + gap);
    return irr::core::rect<irr::f32>(x, y, x + w, y + h);
}

irr::f32 WeatherPanel::contentHeight() const
{
    if (tab == TAB_PRESETS) {
        if (presets.empty()) { return 0; }
        const irr::core::rect<irr::f32> last = presetRect((int)presets.size() - 1);
        return last.LowerRightCorner.Y + scroll[TAB_PRESETS] - list.UpperLeftCorner.Y;
    }
    const std::vector<Row>& rs = rowsOfTab();
    if (rs.empty()) { return 0; }
    const irr::core::rect<irr::f32> last = rowRect((int)rs.size() - 1);
    return last.LowerRightCorner.Y + scroll[tab] - list.UpperLeftCorner.Y;
}

void WeatherPanel::clampScroll()
{
    const irr::f32 maxScroll = std::max(0.0f, contentHeight() - list.getHeight());
    scroll[tab] = irr::core::clamp(scroll[tab], 0.0f, maxScroll);
}

int WeatherPanel::rowAt(const irr::core::position2di& p) const
{
    if (tab == TAB_PRESETS || !list.isPointInside(irr::core::vector2df((irr::f32)p.X, (irr::f32)p.Y))) { return -1; }
    const std::vector<Row>& rs = rowsOfTab();
    for (int i = 0; i < (int)rs.size(); i++) {
        if (rs[i].kind == ROW_SECTION) { continue; }
        if (rowRect(i).isPointInside(irr::core::vector2df((irr::f32)p.X, (irr::f32)p.Y))) { return i; }
    }
    return -1;
}

int WeatherPanel::presetAt(const irr::core::position2di& p) const
{
    if (tab != TAB_PRESETS || !list.isPointInside(irr::core::vector2df((irr::f32)p.X, (irr::f32)p.Y))) { return -1; }
    for (int i = 0; i < (int)presets.size(); i++) {
        if (presetRect(i).isPointInside(irr::core::vector2df((irr::f32)p.X, (irr::f32)p.Y))) { return i; }
    }
    return -1;
}

void WeatherPanel::dragSlider(int rowIndex, irr::s32 mouseX)
{
    const std::vector<Row>& rs = rowsOfTab();
    if (rowIndex < 0 || rowIndex >= (int)rs.size() || rs[rowIndex].kind != ROW_SLIDER) { return; }
    const Row& row = rs[rowIndex];
    const irr::core::rect<irr::f32> track = sliderTrack(rowRect(rowIndex));
    irr::f32 t = ((irr::f32)mouseX - track.UpperLeftCorner.X) / std::max(1.0f, track.getWidth());
    t = irr::core::clamp(t, 0.0f, 1.0f);
    irr::f32 v = row.minValue + t * (row.maxValue - row.minValue);
    if (row.step > 0) { v = floorf(v / row.step + 0.5f) * row.step; }
    setValue(row.param, irr::core::clamp(v, row.minValue, row.maxValue));
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
    switch (m.Event) {
    case irr::EMIE_MOUSE_MOVED:
        if (draggedRow >= 0) { dragSlider(draggedRow, m.X); return true; }
        hoveredClose = closeRect().isPointInside(pf);
        hoveredTab = -1;
        for (int t = 0; t < TAB_COUNT; t++) { if (tabRect(t).isPointInside(pf)) { hoveredTab = t; } }
        hoveredRow = rowAt(p);
        hoveredPreset = presetAt(p);
        return AbsoluteRect.isPointInside(p);
    case irr::EMIE_LMOUSE_PRESSED_DOWN: {
        if (!AbsoluteRect.isPointInside(p)) { return false; }
        if (closeRect().isPointInside(pf)) {
            setVisible(false);
            Environment->removeFocus(this);
            return true;
        }
        for (int t = 0; t < TAB_COUNT; t++) {
            if (tabRect(t).isPointInside(pf)) {
                tab = t;
                selectedRow = -1;
                hoveredRow = -1;
                clampScroll();
                return true;
            }
        }
        const int preset = presetAt(p);
        if (preset >= 0) {
            applyPreset(preset);
            return true;
        }
        const int row = rowAt(p);
        if (row >= 0) {
            selectedRow = row;
            const Row& r = rowsOfTab()[row];
            const irr::core::rect<irr::f32> rr = rowRect(row);
            const irr::f32 cellX = rr.UpperLeftCorner.X + rr.getWidth() * 0.56f;
            if (pf.X >= cellX) {
                if (r.kind == ROW_SLIDER) {
                    draggedRow = row;
                    Environment->setFocus(this); //the moves come here while dragging, even off the row
                    dragSlider(row, m.X);
                }
                else if (r.kind == ROW_CHOICE) {
                    const int n = (int)r.choices.size();
                    const int current = choiceIndex(r.param);
                    const bool forward = pf.X >= (cellX + rr.LowerRightCorner.X) * 0.5f;
                    setChoice(r.param, (current + (forward ? 1 : n - 1)) % n);
                }
            }
        }
        return true;
    }
    case irr::EMIE_LMOUSE_LEFT_UP:
        if (draggedRow >= 0) {
            draggedRow = -1;
            Environment->removeFocus(this); //keys go back to the ship
            return true;
        }
        return AbsoluteRect.isPointInside(p);
    case irr::EMIE_MOUSE_WHEEL:
        if (!AbsoluteRect.isPointInside(p)) { return false; }
        if (list.isPointInside(pf)) {
            scroll[tab] -= m.Wheel * 60.0f * k;
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
    if (!fonts.title) { return; }
    irr::video::IVideoDriver* driver = Environment->getVideoDriver();
    const bridge::Palette& pal = bridge::palette();
    const irr::core::rect<irr::s32>& clip = AbsoluteClippingRect;

    //Window: translucent, so the sea shows through, with a shadow and a hairline edge
    irr::core::rect<irr::s32> shadow = AbsoluteRect;
    shadow += irr::core::position2di(0, (irr::s32)(6 * k));
    bridge::fillRound(driver, shadow, (irr::s32)(10 * k), irr::video::SColor(70, 0, 0, 0), irr::video::SColor(70, 0, 0, 0), 0);
    bridge::fillRound(driver, AbsoluteRect, (irr::s32)(10 * k), withAlpha(pal.panelTop, 236), withAlpha(pal.panelBottom, 236), 0);
    outline(driver, AbsoluteRect, pal.edge, 0);

    //Title, what the weather is doing now, the close box
    const irr::f32 tx = header.UpperLeftCorner.X + 24 * k;
    bridge::drawIcon(driver, bridge::ICON_WEATHER, tx, header.getCenter().Y - 15 * k, 30 * k, pal.accent);
    fonts.title->drawIn(L"M\u00C9T\u00C9O", irr::core::rect<irr::f32>(tx + 42 * k, header.UpperLeftCorner.Y, tx + 300 * k, header.LowerRightCorner.Y),
        pal.text, HudFont::Left, 1.5f * k);
    std::wstring status;
    if (model && model->getWeatherByInstructor()) {
        status = model->isWeatherChanging() ? L"M\u00E9t\u00E9o pilot\u00E9e par l'instructeur  \u00B7  changement en cours" : L"M\u00E9t\u00E9o pilot\u00E9e par l'instructeur";
    }
    else if (model && model->getWeatherFront() != 0) {
        wchar_t text[96];
        swprintf(text, 96, L"Perturbation en cours  \u00B7  %d %%", (int)(model->getWeatherFrontProgress() * 100.0f + 0.5f));
        status = text;
    }
    else if (model && model->isWeatherChanging()) {
        wchar_t text[96];
        swprintf(text, 96, L"Changement de temps en cours  \u00B7  %d %%", (int)(model->getWeatherChangeProgress() * 100.0f + 0.5f));
        status = text;
    }
    else if (chosenPreset >= 0) {
        status = L"Pr\u00E9r\u00E9glage : " + presets[chosenPreset].name;
    }
    else {
        status = L"Conditions personnalis\u00E9es";
    }
    const irr::core::rect<irr::f32> cr = closeRect();
    fonts.caption->drawIn(status, irr::core::rect<irr::f32>(tx + 170 * k, header.UpperLeftCorner.Y, cr.UpperLeftCorner.X - 20 * k, header.LowerRightCorner.Y),
        pal.textDim, HudFont::Right, 0.5f * k);
    if (hoveredClose) { bridge::fillRound(driver, toI(cr), (irr::s32)(5 * k), pal.danger, pal.danger, 0); }
    {
        const irr::video::SColor xc = hoveredClose ? pal.dangerText : pal.textDim;
        const irr::f32 in = cr.getWidth() * 0.32f;
        irr::gui::PanelBatch b;
        b.begin(driver);
        b.line(irr::core::vector2df(cr.UpperLeftCorner.X + in, cr.UpperLeftCorner.Y + in), irr::core::vector2df(cr.LowerRightCorner.X - in, cr.LowerRightCorner.Y - in), 2.2f * k, xc);
        b.line(irr::core::vector2df(cr.LowerRightCorner.X - in, cr.UpperLeftCorner.Y + in), irr::core::vector2df(cr.UpperLeftCorner.X + in, cr.LowerRightCorner.Y - in), 2.2f * k, xc);
        b.flush();
    }
    fill(driver, irr::core::rect<irr::f32>(AbsoluteRect.UpperLeftCorner.X + 1, header.LowerRightCorner.Y - 1, AbsoluteRect.LowerRightCorner.X - 1, header.LowerRightCorner.Y), withAlpha(pal.edge, 160), &clip);

    //Tabs
    for (int t = 0; t < TAB_COUNT; t++) {
        const irr::core::rect<irr::f32> r = tabRect(t);
        const irr::core::rect<irr::f32> rr(r.UpperLeftCorner.X, r.UpperLeftCorner.Y + 8 * k, r.LowerRightCorner.X, r.LowerRightCorner.Y);
        irr::video::SColor face = (t == tab) ? pal.accent : (t == hoveredTab ? pal.keyHover : pal.raised);
        bridge::fillRound(driver, toI(rr), (irr::s32)(3 * k), face, face, 0);
        if (t != tab) { outline(driver, toI(rr), withAlpha(pal.edge, 200), 0); }
        fonts.tab->drawIn(TAB_NAMES[t], rr, t == tab ? pal.accentText : pal.text, HudFont::Centre, 1.2f * k);
    }
    fill(driver, irr::core::rect<irr::f32>(list.UpperLeftCorner.X, tabsBar.LowerRightCorner.Y + 8 * k, info.LowerRightCorner.X, tabsBar.LowerRightCorner.Y + 9 * k),
        withAlpha(pal.edge, 140), &clip);

    //The list (clipped to its own area, it scrolls)
    irr::core::rect<irr::s32> listClip = toI(list);
    listClip.clipAgainst(clip);
    if (tab == TAB_PRESETS) { drawPresets(driver, listClip); }
    else { drawRows(driver, listClip); }

    //Scroll bar, when there is more than fits
    const irr::f32 total = contentHeight();
    if (total > list.getHeight() + 1) {
        const irr::f32 x = list.LowerRightCorner.X - 4 * k;
        fill(driver, irr::core::rect<irr::f32>(x - 1.5f * k, list.UpperLeftCorner.Y, x + 1.5f * k, list.LowerRightCorner.Y), withAlpha(pal.edge, 160), &clip);
        const irr::f32 thumbH = std::max(30 * k, list.getHeight() * list.getHeight() / total);
        const irr::f32 thumbY = list.UpperLeftCorner.Y + (list.getHeight() - thumbH) * scroll[tab] / std::max(1.0f, total - list.getHeight());
        bridge::fillRound(driver, toI(irr::core::rect<irr::f32>(x - 2.5f * k, thumbY, x + 2.5f * k, thumbY + thumbH)), (irr::s32)(2 * k), pal.textDim, pal.textDim, &clip);
    }

    drawInfo(driver);
}

void WeatherPanel::drawRows(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip)
{
    const bridge::Palette& pal = bridge::palette();
    const std::vector<Row>& rs = rowsOfTab();
    for (int i = 0; i < (int)rs.size(); i++) {
        const Row& row = rs[i];
        const irr::core::rect<irr::f32> r = rowRect(i);
        if (r.LowerRightCorner.Y < list.UpperLeftCorner.Y || r.UpperLeftCorner.Y > list.LowerRightCorner.Y) { continue; }
        if (row.kind == ROW_SECTION) {
            const irr::f32 ly = r.LowerRightCorner.Y - 6 * k;
            fonts.section->drawIn(row.label, irr::core::rect<irr::f32>(r.UpperLeftCorner.X + 2 * k, r.UpperLeftCorner.Y, r.LowerRightCorner.X, ly - 2 * k),
                pal.textDim, HudFont::Left, 1.6f * k, &clip);
            fill(driver, irr::core::rect<irr::f32>(r.UpperLeftCorner.X, ly, r.LowerRightCorner.X, ly + 1), withAlpha(pal.edge, 170), &clip);
            continue;
        }
        const bool active = (i == selectedRow) || (i == draggedRow);
        const bool hover = (i == hoveredRow);
        const irr::f32 split = r.UpperLeftCorner.X + r.getWidth() * 0.56f;

        //Label cell: highlighted when it is the row in use
        const irr::core::rect<irr::f32> labelCell(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, split, r.LowerRightCorner.Y);
        irr::video::SColor face = active ? pal.accent : (hover ? mix(pal.raised, pal.accent, 0.25f) : pal.raised);
        bridge::fillRound(driver, toI(labelCell), (irr::s32)(3 * k), withAlpha(face, 235), withAlpha(face, 235), &clip);
        fonts.label->drawIn(row.label, irr::core::rect<irr::f32>(labelCell.UpperLeftCorner.X + 16 * k, labelCell.UpperLeftCorner.Y, labelCell.LowerRightCorner.X - 8 * k, labelCell.LowerRightCorner.Y),
            active ? pal.accentText : pal.text, HudFont::Left, 0.3f * k, &clip);

        //Value cell
        const irr::core::rect<irr::f32> valueCell(split + 4 * k, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y);
        bridge::fillRound(driver, toI(valueCell), (irr::s32)(3 * k), withAlpha(pal.field, 240), withAlpha(pal.field, 240), &clip);
        if (hover || active) { outline(driver, toI(valueCell), withAlpha(pal.accent, 180), &clip); }

        if (row.kind == ROW_SLIDER) {
            fonts.value->drawIn(valueText(row), irr::core::rect<irr::f32>(valueCell.UpperLeftCorner.X + 10 * k, valueCell.UpperLeftCorner.Y, valueCell.UpperLeftCorner.X + 74 * k, valueCell.LowerRightCorner.Y),
                pal.text, HudFont::Left, 0, &clip);
            const irr::core::rect<irr::f32> track = sliderTrack(r);
            const irr::f32 t = irr::core::clamp((value(row.param) - row.minValue) / std::max(0.001f, row.maxValue - row.minValue), 0.0f, 1.0f);
            const irr::f32 kx = track.UpperLeftCorner.X + t * track.getWidth();
            fill(driver, track, withAlpha(pal.edge, 220), &clip);
            fill(driver, irr::core::rect<irr::f32>(track.UpperLeftCorner.X, track.UpperLeftCorner.Y, kx, track.LowerRightCorner.Y), pal.accent, &clip);
            //Diamond knob
            const irr::f32 ks = (active ? 9.0f : 7.5f) * k;
            const irr::f32 cy = track.getCenter().Y;
            irr::gui::PanelBatch b;
            b.begin(driver);
            b.quad(irr::core::vector2df(kx, cy - ks - 1.5f * k), irr::core::vector2df(kx + ks + 1.5f * k, cy), irr::core::vector2df(kx, cy + ks + 1.5f * k), irr::core::vector2df(kx - ks - 1.5f * k, cy), pal.field);
            b.quad(irr::core::vector2df(kx, cy - ks), irr::core::vector2df(kx + ks, cy), irr::core::vector2df(kx, cy + ks), irr::core::vector2df(kx - ks, cy), active ? pal.accent : pal.text);
            b.flush();
        }
        else if (row.kind == ROW_CHOICE) {
            const int n = (int)row.choices.size();
            const int current = irr::core::clamp(choiceIndex(row.param), 0, n - 1);
            const irr::core::rect<irr::f32> textBox(valueCell.UpperLeftCorner.X, valueCell.UpperLeftCorner.Y - 3 * k, valueCell.LowerRightCorner.X, valueCell.LowerRightCorner.Y - 3 * k);
            fonts.value->drawIn(row.choices[current], textBox, pal.text, HudFont::Centre, 0, &clip);
            //Arrows at both ends, one dash per option underneath
            const irr::f32 cy = valueCell.getCenter().Y, a = 5 * k;
            irr::gui::PanelBatch b;
            b.begin(driver);
            const irr::f32 lx = valueCell.UpperLeftCorner.X + 14 * k, rx = valueCell.LowerRightCorner.X - 14 * k;
            b.tri(irr::core::vector2df(lx - a * 0.6f, cy), irr::core::vector2df(lx + a * 0.6f, cy - a), irr::core::vector2df(lx + a * 0.6f, cy + a), pal.textDim);
            b.tri(irr::core::vector2df(rx + a * 0.6f, cy), irr::core::vector2df(rx - a * 0.6f, cy + a), irr::core::vector2df(rx - a * 0.6f, cy - a), pal.textDim);
            const irr::f32 dw = 12 * k, dg = 3 * k;
            const irr::f32 dx0 = valueCell.getCenter().X - (n * dw + (n - 1) * dg) * 0.5f;
            const irr::f32 dy = valueCell.LowerRightCorner.Y - 7 * k;
            for (int j = 0; j < n; j++) {
                b.rect(irr::core::rect<irr::f32>(dx0 + j * (dw + dg), dy, dx0 + j * (dw + dg) + dw, dy + 2.5f * k), j == current ? pal.accent : withAlpha(pal.textDim, 150));
            }
            b.flush();
        }
    }
}

void WeatherPanel::drawPresets(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip)
{
    const bridge::Palette& pal = bridge::palette();
    const bool day = bridge::currentMode() == bridge::MODE_DAY;
    const irr::video::SColor paper = day ? irr::video::SColor(255, 214, 220, 228) : mix(pal.field, pal.raised, 0.5f);
    const irr::video::SColor ink = day ? irr::video::SColor(255, 30, 38, 48) : pal.text;
    for (int i = 0; i < (int)presets.size(); i++) {
        const irr::core::rect<irr::f32> r = presetRect(i);
        if (r.LowerRightCorner.Y < list.UpperLeftCorner.Y || r.UpperLeftCorner.Y > list.LowerRightCorner.Y) { continue; }
        const bool chosen = (i == chosenPreset);
        const bool hover = (i == hoveredPreset);
        //Caption bar over the picture
        const irr::core::rect<irr::f32> cap(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.UpperLeftCorner.Y + 30 * k);
        const irr::video::SColor capFace = chosen ? pal.accent : (hover ? mix(pal.raised, pal.accent, 0.3f) : pal.raised);
        fill(driver, cap, withAlpha(capFace, 240), &clip);
        fonts.caption->drawIn(hudUpper(presets[i].name), cap, chosen ? pal.accentText : pal.text, HudFont::Centre, 1.0f * k, &clip);
        //Picture
        const irr::core::rect<irr::f32> pic(r.UpperLeftCorner.X, cap.LowerRightCorner.Y + 3 * k, r.LowerRightCorner.X, r.LowerRightCorner.Y);
        fill(driver, pic, chosen || hover ? pal.accent : withAlpha(pal.edge, 230), &clip);
        const irr::f32 frame = (chosen ? 3.0f : 2.0f) * k;
        const irr::core::rect<irr::f32> inner(pic.UpperLeftCorner.X + frame, pic.UpperLeftCorner.Y + frame, pic.LowerRightCorner.X - frame, pic.LowerRightCorner.Y - frame);
        fill(driver, inner, paper, &clip);
        const irr::f32 side = std::min(inner.getWidth(), inner.getHeight()) * 0.86f;
        const irr::core::vector2df c = inner.getCenter();
        const irr::core::rect<irr::f32> iconBox(c.X - side * 0.5f, c.Y - side * 0.5f, c.X + side * 0.5f, c.Y + side * 0.5f);
        if (iconBox.UpperLeftCorner.Y >= list.UpperLeftCorner.Y - 1 && iconBox.LowerRightCorner.Y <= list.LowerRightCorner.Y + 1) {
            drawWeatherIcon(driver, presets[i].icon, iconBox, ink, paper);
        }
    }
}

int WeatherPanel::currentWeatherIcon() const
{
    if (!model) { return WX_SUN; }
    const irr::f32 rain = model->getRain(), snow = model->getSnow(), dust = model->getDust();
    const irr::f32 vis = model->getVisibility(), cloud = model->getCloudCover(), wind = model->getWindSpeedBase();
    const bool storm = model->getSignificantWeather() == 2 || (model->getSignificantWeather() == 0 && model->getWeather() >= 3.5f);
    if (storm && (rain > 3 || model->getWeather() >= 5)) { return model->getWeather() >= 5 ? WX_STORM : WX_THUNDER; }
    if (snow > 0.6f && wind > 30) { return WX_BLIZZARD; }
    if (snow > 0.4f) { return WX_SNOW; }
    if (snow > 0.05f) { return WX_SNOW_LIGHT; }
    if (dust > 0.6f) { return WX_DUSTSTORM; }
    if (dust > 0.1f) { return WX_DUST; }
    if (vis < 0.6f) { return WX_FOG; }
    if (rain > 4) { return WX_RAIN; }
    if (rain > 0.5f) { return WX_RAIN_LIGHT; }
    if (wind >= 34) { return WX_GALE; }
    if (cloud > 0.75f) { return WX_OVERCAST; }
    if (cloud > 0.4f) { return WX_CLOUDY; }
    if (cloud > 0.12f) { return WX_FEW; }
    return WX_SUN;
}

void WeatherPanel::drawInfo(irr::video::IVideoDriver* driver)
{
    const bridge::Palette& pal = bridge::palette();
    const irr::core::rect<irr::s32>& clip = AbsoluteClippingRect;

    //What the card is about: the row or tile under the mouse, else the one last used, else the tab
    std::wstring title, help;
    int icon = currentWeatherIcon();
    if (tab == TAB_PRESETS) {
        const int p = hoveredPreset >= 0 ? hoveredPreset : chosenPreset;
        if (p >= 0) { title = presets[p].name; help = presets[p].help; icon = presets[p].icon; }
        else {
            title = L"Pr\u00E9r\u00E9glages";
            help = L"Un clic r\u00E8gle tout le temps d'un coup : ciel, vent, mer, visibilit\u00E9 et pluie. Pour un changement petit \u00E0 petit, choisissez une dur\u00E9e dans l'onglet \u00C9volution.";
        }
    }
    else {
        const std::vector<Row>& rs = rowsOfTab();
        const int r = hoveredRow >= 0 ? hoveredRow : selectedRow;
        if (r >= 0 && r < (int)rs.size()) {
            title = rs[r].title;
            help = rs[r].help;
            if (rs[r].icon >= 0 && tab != TAB_CONDITIONS) { icon = rs[r].icon; }
        }
        else if (tab == TAB_CONDITIONS) {
            title = L"Conditions";
            help = L"Chaque \u00E9l\u00E9ment du temps se r\u00E8gle avec son curseur. Le changement est imm\u00E9diat.";
        }
        else if (tab == TAB_EVOLUTION) {
            title = L"\u00C9volution";
            help = L"Comment le temps change pendant l'exercice : la dur\u00E9e du changement quand vous choisissez un pr\u00E9r\u00E9glage, et le passage d'une perturbation.";
            icon = WX_FRONT;
        }
        else {
            title = L"Heure";
            help = L"L'heure de la lumi\u00E8re : jour, aube, cr\u00E9puscule ou nuit.";
            icon = WX_CLOCK;
        }
    }

    //Card: title, a rule, the explanation
    const irr::f32 cardBottom = tile.UpperLeftCorner.Y - 16 * k;
    const irr::core::rect<irr::f32> card(info.UpperLeftCorner.X, info.UpperLeftCorner.Y, info.LowerRightCorner.X, cardBottom);
    bridge::fillRound(driver, toI(card), (irr::s32)(4 * k), withAlpha(pal.raised, 220), withAlpha(pal.panelBottom, 120), &clip);
    const irr::f32 px = card.UpperLeftCorner.X + 14 * k;
    fonts.infoTitle->drawIn(hudUpper(title), irr::core::rect<irr::f32>(px, card.UpperLeftCorner.Y + 6 * k, card.LowerRightCorner.X - 12 * k, card.UpperLeftCorner.Y + 46 * k),
        pal.text, HudFont::Left, 0.8f * k, &clip);
    fill(driver, irr::core::rect<irr::f32>(px, card.UpperLeftCorner.Y + 50 * k, card.LowerRightCorner.X - 14 * k, card.UpperLeftCorner.Y + 50 * k + std::max(1.0f, 1.5f * k)), pal.text, &clip);
    const std::vector<std::wstring> lines = fonts.infoText->wrap(help, card.getWidth() - 28 * k);
    irr::f32 ly = card.UpperLeftCorner.Y + 60 * k;
    const irr::f32 lh = fonts.infoText->lineHeight() + 2 * k;
    for (size_t i = 0; i < lines.size() && ly + lh <= card.LowerRightCorner.Y; i++, ly += lh) {
        fonts.infoText->drawIn(lines[i], irr::core::rect<irr::f32>(px, ly, card.LowerRightCorner.X - 12 * k, ly + lh), pal.textDim, HudFont::Left, 0, &clip);
    }

    //Picture tile, framed, with its caption
    const bool day = bridge::currentMode() == bridge::MODE_DAY;
    const irr::video::SColor paper = day ? irr::video::SColor(255, 214, 220, 228) : mix(pal.field, pal.raised, 0.5f);
    const irr::video::SColor ink = day ? irr::video::SColor(255, 30, 38, 48) : pal.text;
    fill(driver, tile, withAlpha(pal.edge, 240), &clip);
    const irr::f32 fr = 5 * k;
    const irr::core::rect<irr::f32> inner(tile.UpperLeftCorner.X + fr, tile.UpperLeftCorner.Y + fr, tile.LowerRightCorner.X - fr, tile.LowerRightCorner.Y - fr);
    fill(driver, inner, paper, &clip);
    fonts.section->drawIn(tab == TAB_PRESETS && (hoveredPreset >= 0 || chosenPreset >= 0) ? L"PR\u00C9R\u00C9GLAGE" : L"TEMPS ACTUEL",
        irr::core::rect<irr::f32>(inner.UpperLeftCorner.X + 8 * k, inner.UpperLeftCorner.Y + 4 * k, inner.LowerRightCorner.X, inner.UpperLeftCorner.Y + 22 * k),
        withAlpha(ink, 150), HudFont::Left, 1.0f * k, &clip);
    const irr::f32 side = inner.getWidth() * 0.72f;
    const irr::core::vector2df c(inner.getCenter().X, inner.getCenter().Y + 8 * k);
    drawWeatherIcon(driver, icon, irr::core::rect<irr::f32>(c.X - side * 0.5f, c.Y - side * 0.5f, c.X + side * 0.5f, c.Y + side * 0.5f), ink, paper);

    //Beside the tile: the weather as it is now (gusts and all)
    if (!model) { return; }
    const irr::f32 rx = tile.LowerRightCorner.X + 16 * k;
    if (rx > info.LowerRightCorner.X - 60 * k) { return; }
    irr::f32 ry = tile.UpperLeftCorner.Y;
    auto readout = [&](const std::wstring& label, const std::wstring& text) {
        fonts.section->drawIn(label, irr::core::rect<irr::f32>(rx, ry, info.LowerRightCorner.X, ry + 18 * k), pal.textDim, HudFont::Left, 1.2f * k, &clip);
        fonts.value->drawIn(text, irr::core::rect<irr::f32>(rx, ry + 18 * k, info.LowerRightCorner.X, ry + 42 * k), pal.text, HudFont::Left, 0, &clip);
        ry += 50 * k;
    };
    wchar_t text[96];
    swprintf(text, 96, L"%03d\u00B0  \u00B7  %d kn", ((int)(model->getWindDirection() + 0.5f)) % 360, (int)(model->getWindSpeed() + 0.5f));
    readout(L"VENT", text);
    const irr::f32 vis = model->getVisibility();
    if (vis >= 1.0f) { swprintf(text, 96, L"%.1f NM", vis); }
    else { swprintf(text, 96, L"%d m", (int)(vis * 1852.0f / 10.0f + 0.5f) * 10); }
    readout(L"VISIBILIT\u00C9", text);
    swprintf(text, 96, L"%.1f", model->getWeather());
    readout(L"MER", text);
    swprintf(text, 96, L"%d %%", (int)(model->getCloudCover() * 100.0f + 0.5f));
    readout(L"NUAGES", text);
    if (model->isWeatherChanging()) {
        //Progress of the change under way
        const irr::f32 prog = model->getWeatherChangeProgress();
        const irr::core::rect<irr::f32> bar(rx, ry + 4 * k, std::min(info.LowerRightCorner.X, rx + 160 * k), ry + 9 * k);
        fill(driver, bar, withAlpha(pal.edge, 200), &clip);
        fill(driver, irr::core::rect<irr::f32>(bar.UpperLeftCorner.X, bar.UpperLeftCorner.Y, bar.UpperLeftCorner.X + bar.getWidth() * prog, bar.LowerRightCorner.Y), pal.accent, &clip);
        fonts.section->drawIn(model->getWeatherFront() ? L"PERTURBATION EN COURS" : L"\u00C9VOLUTION EN COURS",
            irr::core::rect<irr::f32>(rx, ry + 12 * k, info.LowerRightCorner.X, ry + 30 * k), pal.accent, HudFont::Left, 1.2f * k, &clip);
    }
}

//-------------------------------------------------------------------------------------------------
//Weather pictures
//-------------------------------------------------------------------------------------------------

void WeatherPanel::drawWeatherIcon(irr::video::IVideoDriver* driver, int icon, const irr::core::rect<irr::f32>& box,
    irr::video::SColor ink, irr::video::SColor paper)
{
    const irr::f32 s = box.getWidth();
    const irr::f32 ox = box.UpperLeftCorner.X, oy = box.UpperLeftCorner.Y;
    auto P = [&](irr::f32 x, irr::f32 y) { return irr::core::vector2df(ox + x * s, oy + y * s); };
    irr::gui::PanelBatch b;
    b.begin(driver);

    //A cloud: three puffs on a flat base. Outlined (a lighter weather) or solid (a heavier one).
    auto cloudShape = [&](irr::f32 cx, irr::f32 cy, irr::f32 sc, irr::f32 grow, irr::video::SColor col) {
        b.disc(P(cx - 0.22f * sc, cy + 0.02f * sc), (0.19f * sc + grow) * s, col, col);
        b.disc(P(cx + 0.02f * sc, cy - 0.10f * sc), (0.27f * sc + grow) * s, col, col);
        b.disc(P(cx + 0.27f * sc, cy + 0.04f * sc), (0.17f * sc + grow) * s, col, col);
        b.rect(irr::core::rect<irr::f32>(P(cx - 0.22f * sc, cy + 0.02f * sc - grow).X, P(0, cy + 0.02f * sc).Y,
            P(cx + 0.27f * sc, 0).X, P(0, cy + 0.21f * sc + grow).Y), col);
    };
    auto cloud = [&](irr::f32 cx, irr::f32 cy, irr::f32 sc, bool solid) {
        const irr::f32 t = 0.035f * sc;
        cloudShape(cx, cy, sc, solid ? 0.0f : t, ink);
        if (!solid) { cloudShape(cx, cy, sc, -0.0f, paper); }
    };
    auto sun = [&](irr::f32 cx, irr::f32 cy, irr::f32 r) {
        b.disc(P(cx, cy), r * s, ink, ink);
        for (int i = 0; i < 8; i++) {
            const irr::f32 a = i * 45.0f * irr::core::DEGTORAD;
            b.line(P(cx + sinf(a) * r * 1.45f, cy - cosf(a) * r * 1.45f), P(cx + sinf(a) * r * 1.95f, cy - cosf(a) * r * 1.95f), r * 0.28f * s, ink);
        }
    };
    auto drops = [&](int n, irr::f32 x0, irr::f32 x1, irr::f32 y0, irr::f32 y1) {
        for (int i = 0; i < n; i++) {
            const irr::f32 x = x0 + (x1 - x0) * (n > 1 ? (irr::f32)i / (n - 1) : 0.5f);
            b.line(P(x + 0.04f, y0), P(x - 0.04f, y1), 0.028f * s, ink);
        }
    };
    auto flake = [&](irr::f32 cx, irr::f32 cy, irr::f32 r) {
        const irr::f32 w = std::max(1.5f, r * 0.13f * s);
        for (int i = 0; i < 6; i++) {
            const irr::f32 a = i * 60.0f * irr::core::DEGTORAD;
            const irr::core::vector2df tip = P(cx + sinf(a) * r, cy - cosf(a) * r);
            b.line(P(cx, cy), tip, w, ink);
            const irr::core::vector2df mid = P(cx + sinf(a) * r * 0.58f, cy - cosf(a) * r * 0.58f);
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                const irr::f32 a2 = a + sgn * 0.75f;
                b.line(mid, irr::core::vector2df(mid.X + sinf(a2) * r * 0.32f * s, mid.Y - cosf(a2) * r * 0.32f * s), w * 0.8f, ink);
            }
        }
        b.disc(P(cx, cy), r * 0.14f * s, ink, ink);
    };
    auto bolt = [&](irr::f32 cx, irr::f32 cy, irr::f32 sc) {
        b.quad(P(cx + 0.02f * sc, cy - 0.30f * sc), P(cx + 0.14f * sc, cy - 0.30f * sc), P(cx + 0.02f * sc, cy + 0.02f * sc), P(cx - 0.12f * sc, cy + 0.02f * sc), ink);
        b.quad(P(cx - 0.06f * sc, cy - 0.02f * sc), P(cx + 0.10f * sc, cy - 0.02f * sc), P(cx - 0.08f * sc, cy + 0.34f * sc), P(cx - 0.08f * sc, cy + 0.20f * sc), ink);
    };
    auto windLines = [&](irr::f32 x0, irr::f32 y0, irr::f32 sc) {
        const irr::f32 w = 0.045f * sc * s;
        const irr::f32 lens[3] = { 0.62f, 0.78f, 0.48f };
        for (int i = 0; i < 3; i++) {
            const irr::f32 y = y0 + i * 0.16f * sc;
            b.line(P(x0, y), P(x0 + lens[i] * sc, y), w, ink);
            b.disc(P(x0, y), w * 0.5f, ink, ink);
            //curl at the end
            b.sector(P(x0 + lens[i] * sc, y - 0.06f * sc), 0.06f * sc * s - w * 0.5f, 0.06f * sc * s + w * 0.5f, -10.0f, 180.0f, ink, ink);
        }
    };
    auto fogBars = [&](irr::f32 cx, irr::f32 cy, irr::f32 sc) {
        const irr::f32 widths[6] = { 0.30f, 0.52f, 0.66f, 0.70f, 0.66f, 0.50f };
        for (int i = 0; i < 6; i++) {
            const irr::f32 y = cy + (i - 2.5f) * 0.075f * sc;
            const irr::f32 w = widths[i] * sc;
            b.line(P(cx - w * 0.5f, y), P(cx + w * 0.5f, y), 0.042f * sc * s, ink);
        }
    };
    auto dustCloud = [&](irr::f32 cx, irr::f32 cy, irr::f32 sc, bool dense) {
        //A cloud outline made of dots, a ground line underneath
        const irr::f32 step = dense ? 0.045f : 0.06f;
        for (irr::f32 y = cy - 0.36f * sc; y <= cy + 0.2f * sc; y += step * sc) {
            for (irr::f32 x = cx - 0.42f * sc; x <= cx + 0.46f * sc; x += step * sc) {
                const irr::f32 dx1 = (x - (cx - 0.22f * sc)) / (0.19f * sc), dy1 = (y - (cy + 0.02f * sc)) / (0.19f * sc);
                const irr::f32 dx2 = (x - (cx + 0.02f * sc)) / (0.27f * sc), dy2 = (y - (cy - 0.10f * sc)) / (0.27f * sc);
                const irr::f32 dx3 = (x - (cx + 0.27f * sc)) / (0.17f * sc), dy3 = (y - (cy + 0.04f * sc)) / (0.17f * sc);
                const bool inBase = x >= cx - 0.22f * sc && x <= cx + 0.27f * sc && y >= cy + 0.02f * sc && y <= cy + 0.21f * sc;
                if (inBase || dx1 * dx1 + dy1 * dy1 <= 1 || dx2 * dx2 + dy2 * dy2 <= 1 || dx3 * dx3 + dy3 * dy3 <= 1) {
                    const irr::f32 d = 0.011f * sc;
                    b.rect(irr::core::rect<irr::f32>(P(x - d, y - d), P(x + d, y + d)), ink); //small squares: hundreds of them
                }
            }
        }
        b.line(P(cx - 0.42f * sc, cy + 0.29f * sc), P(cx + 0.46f * sc, cy + 0.29f * sc), 0.035f * sc * s, ink);
    };
    auto waves = [&](irr::f32 y, irr::f32 amp, irr::f32 w) {
        irr::core::vector2df prev = P(0.1f, y);
        for (int i = 1; i <= 24; i++) {
            const irr::f32 x = 0.1f + 0.8f * i / 24.0f;
            const irr::core::vector2df cur = P(x, y + amp * sinf(i / 24.0f * 2.0f * irr::core::PI * 2.0f));
            b.line(prev, cur, w * s, ink);
            prev = cur;
        }
    };

    switch (icon) {
    case WX_SUN: sun(0.5f, 0.5f, 0.17f); break;
    case WX_FEW: sun(0.36f, 0.36f, 0.12f); cloud(0.58f, 0.6f, 0.75f, false); break;
    case WX_CLOUDY: sun(0.33f, 0.38f, 0.11f); cloud(0.55f, 0.56f, 0.95f, false); break;
    case WX_OVERCAST: cloud(0.5f, 0.52f, 1.05f, true); break;
    case WX_RAIN_LIGHT: cloud(0.5f, 0.4f, 0.9f, false); drops(4, 0.32f, 0.68f, 0.68f, 0.86f); break;
    case WX_RAIN: cloud(0.5f, 0.4f, 0.9f, true); drops(6, 0.28f, 0.72f, 0.68f, 0.88f); break;
    case WX_FOG: fogBars(0.5f, 0.5f, 1.0f); break;
    case WX_SNOW_LIGHT: sun(0.6f, 0.3f, 0.08f); cloud(0.68f, 0.38f, 0.5f, false); flake(0.32f, 0.66f, 0.15f); break;
    case WX_SNOW: flake(0.5f, 0.5f, 0.32f); break;
    case WX_THUNDER: cloud(0.5f, 0.38f, 0.9f, true); drops(2, 0.3f, 0.36f, 0.68f, 0.86f); drops(2, 0.64f, 0.7f, 0.68f, 0.86f); bolt(0.5f, 0.76f, 0.5f); break;
    case WX_BLIZZARD: windLines(0.08f, 0.38f, 0.45f); flake(0.6f, 0.5f, 0.26f); break;
    case WX_DUST: dustCloud(0.5f, 0.52f, 1.0f, false); break;
    case WX_DUSTSTORM: dustCloud(0.55f, 0.5f, 0.85f, true); windLines(0.06f, 0.36f, 0.35f); break;
    case WX_GALE: windLines(0.16f, 0.32f, 0.9f); break;
    case WX_STORM: cloud(0.5f, 0.32f, 0.8f, true); bolt(0.5f, 0.66f, 0.42f); waves(0.84f, 0.035f, 0.035f); break;
    case WX_WIND: windLines(0.16f, 0.32f, 0.9f); break;
    case WX_GUST: windLines(0.12f, 0.28f, 0.7f); windLines(0.3f, 0.58f, 0.55f); break;
    case WX_SEA: waves(0.38f, 0.05f, 0.04f); waves(0.56f, 0.05f, 0.04f); waves(0.74f, 0.05f, 0.04f); break;
    case WX_CLOCK: {
        b.sector(P(0.5f, 0.5f), 0.31f * s, 0.36f * s, 0, 360, ink, ink);
        b.line(P(0.5f, 0.5f), P(0.5f, 0.27f), 0.05f * s, ink);
        b.line(P(0.5f, 0.5f), P(0.66f, 0.58f), 0.05f * s, ink);
        b.disc(P(0.5f, 0.5f), 0.045f * s, ink, ink);
        break;
    }
    case WX_FRONT: {
        //Cold front: a line with its triangles, a cloud behind
        cloud(0.55f, 0.32f, 0.6f, false);
        b.line(P(0.1f, 0.72f), P(0.9f, 0.62f), 0.04f * s, ink);
        for (int i = 0; i < 4; i++) {
            const irr::f32 x = 0.18f + i * 0.19f;
            const irr::f32 y = 0.72f - (x - 0.1f) * 0.125f;
            b.tri(P(x, y), P(x + 0.12f, y - 0.015f), P(x + 0.065f, y - 0.11f), ink);
        }
        break;
    }
    case WX_COMPASS: {
        b.sector(P(0.5f, 0.5f), 0.32f * s, 0.36f * s, 0, 360, ink, ink);
        b.tri(P(0.5f, 0.16f), P(0.57f, 0.5f), P(0.43f, 0.5f), ink);
        b.tri(P(0.5f, 0.84f), P(0.43f, 0.5f), P(0.57f, 0.5f), withAlpha(ink, 120));
        break;
    }
    default: break;
    }
    b.flush();
}
