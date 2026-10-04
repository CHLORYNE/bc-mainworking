/* SCENARIO INCENDIE - see FireHelp.hpp. Source kept ASCII (\u escapes). */
#include "FireHelp.hpp"

#include <algorithm>
#include <cwchar>
#include <utility>
#include <vector>

namespace {

std::wstring tPlus(float secs)
{
    if (secs < 0.0f) { secs = 0.0f; }
    int s = (int)(secs + 0.5f);
    wchar_t b[32];
    swprintf(b, 32, L"T+%d:%02d", s / 60, s % 60);
    return b;
}

std::wstring dur(float secs)
{
    if (secs < 0.0f) { secs = 0.0f; }
    int s = (int)(secs + 0.5f);
    wchar_t b[32];
    if (s >= 60) { swprintf(b, 32, L"%d min %02d s", s / 60, s % 60); }
    else { swprintf(b, 32, L"%d s", s); }
    return b;
}

float lastSurvivorTime(const IncidentConfig& c)
{
    size_t n = c.survivors.size();
    return c.abandonTime + c.survivorInterval * (n > 0 ? (float)(n - 1) : 0.0f);
}

bool byTime(const std::pair<float, std::wstring>& a, const std::pair<float, std::wstring>& b)
{
    return a.first < b.first;
}

} // namespace

namespace FireHelp {

std::wstring title(Field f)
{
    switch (f) {
    case Field_FireDuration:      return L"DUR\u00C9E DE L'INCENDIE (FireDuration)";
    case Field_SinkLeadTime:      return L"DUR\u00C9E DU NAUFRAGE (SinkLeadTime)";
    case Field_FireSpreadTime:    return L"FEU G\u00C9N\u00C9RALIS\u00C9 (FireSpreadTime)";
    case Field_AbandonTime:       return L"ABANDON DU NAVIRE (AbandonTime)";
    case Field_SurvivorInterval:  return L"INTERVALLE ENTRE NAUFRAG\u00C9S (SurvivorInterval)";
    case Field_PermanentListTime: return L"G\u00CETE PERMANENTE (PermanentListTime)";
    case Field_HeloDelay:         return L"D\u00C9LAI H\u00C9LICOPT\u00C8RES (HeloDelay)";
    case Field_HeloSpeed:         return L"VITESSE H\u00C9LICOPT\u00C8RES (HeloSpeed)";
    case Field_SarBoatDelay:      return L"APPAREILLAGE DE LA VEDETTE (SarBoatDelay)";
    default:                      return L"";
    }
}

std::wstring explanation(Field f)
{
    switch (f) {
    case Field_FireDuration:
        return L"Temps total entre la mise \u00E0 feu (Ctrl+F) et le moment o\u00F9 le navire a enti\u00E8rement "
               L"coul\u00E9, si le feu n'est pas \u00E9teint. Le naufrage occupe la fin de cette dur\u00E9e.";
    case Field_SinkLeadTime:
        return L"Dur\u00E9e de la descente du navire, prise \u00C0 LA FIN de la dur\u00E9e de l'incendie. "
               L"Il commence \u00E0 couler \u00E0 T+ (dur\u00E9e de l'incendie - dur\u00E9e du naufrage) : l'exercice est "
               L"alors perdu et l'\u00E9ch\u00E9ance du stagiaire tombe \u00E0 00:00. "
               L"Une grande valeur fait couler le navire T\u00D4T (04:00 et 03:30 = \u00E9chec \u00E0 T+00:30). "
               L"Habituellement 00:30 \u00E0 01:30.";
    case Field_FireSpreadTime:
        return L"Temps pour passer d'un petit foyer central \u00E0 tout le pont en feu. Le feu (flammes, fum\u00E9e, "
               L"lueur, bruit) grandit pendant cette dur\u00E9e. N'avance pas le naufrage.";
    case Field_AbandonTime:
        return L"L'alarme d'abandon remplace l'alarme incendie et les naufrag\u00E9s commencent \u00E0 \u00EAtre mis "
               L"\u00E0 l'eau, dans l'ordre de l'onglet Naufrag\u00E9s. Doit pr\u00E9c\u00E9der le d\u00E9but du naufrage.";
    case Field_SurvivorInterval:
        return L"Secondes entre deux mises \u00E0 l'eau. Le dernier naufrag\u00E9 est \u00E0 l'eau \u00E0 "
               L"T+ abandon + intervalle x (nombre - 1). Les vedettes partent \u00E0 partir de ce moment.";
    case Field_PermanentListTime:
        return L"Feu \u00E9teint AVANT ce temps : le navire se redresse. Feu \u00E9teint APR\u00C8S : navire sauv\u00E9 "
               L"mais avec une g\u00EEte permanente (avarie). Sans effet une fois le naufrage commenc\u00E9.";
    case Field_HeloDelay:
        return L"Compt\u00E9 depuis l'appel OSC du stagiaire (3e Ctrl+A), PAS depuis la mise \u00E0 feu. "
               L"Sans appel, les h\u00E9licopt\u00E8res ne viennent pas. 00:00 = sur zone imm\u00E9diatement.";
    case Field_HeloSpeed:
        return L"Vitesse de transit en n\u0153uds, vers la zone puis vers l'h\u00E9lisurface. Avec une base, "
               L"l'h\u00E9lico d\u00E9colle assez t\u00F4t pour \u00EAtre sur zone \u00E0 l'heure pr\u00E9vue.";
    case Field_SarBoatDelay:
        return L"Compt\u00E9 depuis la mise \u00E0 l'eau du DERNIER naufrag\u00E9, pas depuis la mise \u00E0 feu. "
               L"00:00 = la vedette appareille d\u00E8s que tout le monde est \u00E0 l'eau.";
    default:
        return L"";
    }
}

std::wstring tooltip(Field f)
{
    switch (f) {
    case Field_FireDuration:      return L"Mise \u00E0 feu -> navire enti\u00E8rement coul\u00E9 (mm:ss)";
    case Field_SinkLeadTime:      return L"Dur\u00E9e de la descente, \u00E0 la fin. Grande valeur = naufrage t\u00F4t";
    case Field_FireSpreadTime:    return L"Mise \u00E0 feu -> tout le pont embras\u00E9 (mm:ss)";
    case Field_AbandonTime:       return L"Mise \u00E0 feu -> alarme d'abandon, premiers naufrag\u00E9s (mm:ss)";
    case Field_SurvivorInterval:  return L"Secondes entre deux mises \u00E0 l'eau";
    case Field_PermanentListTime: return L"\u00C9teint avant = redress\u00E9, apr\u00E8s = g\u00EEte permanente";
    case Field_HeloDelay:         return L"Appel OSC (3e Ctrl+A) -> h\u00E9licopt\u00E8res sur zone (mm:ss)";
    case Field_HeloSpeed:         return L"Vitesse de transit (n\u0153uds)";
    case Field_SarBoatDelay:      return L"Dernier naufrag\u00E9 \u00E0 l'eau -> appareillage (mm:ss)";
    default:                      return L"";
    }
}

std::wstring timeline(const IncidentConfig& c)
{
    float sinkStart = c.sinkStartTime();
    float lastIn = lastSurvivorTime(c);
    size_t n = c.survivors.size();

    // Events, shown in time order (a bad set of values makes the order itself telling).
    std::vector<std::pair<float, std::wstring> > ev;
    ev.push_back(std::make_pair(0.0f, std::wstring(L"mise \u00E0 feu (Ctrl+F)")));
    std::wstring ab = L"abandon du navire";
    if (n > 0) {
        wchar_t b[64];
        swprintf(b, 64, L", %u mis \u00E0 l'eau jusqu'\u00E0 ", (unsigned)n);
        ab += b + tPlus(lastIn);
    }
    ev.push_back(std::make_pair(c.abandonTime, ab));
    ev.push_back(std::make_pair(c.fireSpreadTime, std::wstring(L"feu g\u00E9n\u00E9ralis\u00E9")));
    ev.push_back(std::make_pair(c.permanentListTime, std::wstring(L"au-del\u00E0 : g\u00EEte permanente si \u00E9teint")));
    ev.push_back(std::make_pair(sinkStart, std::wstring(L"d\u00E9but du naufrage = \u00C9CHEC si le feu n'est pas \u00E9teint")));
    ev.push_back(std::make_pair(c.fireDuration, std::wstring(L"navire enti\u00E8rement coul\u00E9")));
    std::stable_sort(ev.begin(), ev.end(), byTime);

    std::wstring t = L"Chronologie avec les valeurs actuelles :\n";
    for (size_t i = 0; i < ev.size(); i++) { t += L"  " + tPlus(ev[i].first) + L"  " + ev[i].second + L"\n"; }
    t += L"  Temps disponible pour \u00E9teindre : " + dur(sinkStart) + L"\n";

    // Warnings
    std::wstring w;
    if (c.sinkLeadTime >= c.fireDuration) {
        w += L"  ! La dur\u00E9e du naufrage d\u00E9passe la dur\u00E9e avant naufrage : "
             L"le navire coule d\u00E8s la mise \u00E0 feu.\n";
    }
    else if (sinkStart < c.abandonTime) {
        w += L"  ! Le naufrage commence (" + tPlus(sinkStart) + L") avant l'abandon ("
             + tPlus(c.abandonTime) + L"). R\u00E9duisez la dur\u00E9e du naufrage.\n";
    }
    else if (n > 0 && sinkStart < lastIn) {
        w += L"  ! Le naufrage commence (" + tPlus(sinkStart) + L") avant que le dernier naufrag\u00E9 "
             L"soit \u00E0 l'eau (" + tPlus(lastIn) + L").\n";
    }
    if (c.fireSpreadTime > sinkStart) {
        w += L"  ! Le feu n'aura pas fini de se propager quand le navire commence \u00E0 couler.\n";
    }
    if (sinkStart > 0.0f && sinkStart < 60.0f) {
        w += L"  ! Moins d'une minute pour \u00E9teindre : l'exercice est tr\u00E8s court.\n";
    }
    if (!w.empty()) { t += L"\nAttention :\n" + w; }
    return t;
}

std::wstring fullText(Field f, const IncidentConfig& c)
{
    std::wstring t;
    if (f != Field_None) { t = title(f) + L"\n\n" + explanation(f) + L"\n\n"; }
    return t + timeline(c);
}

bool hasBlockingProblem(const IncidentConfig& c)
{
    float sinkStart = c.sinkStartTime();
    if (c.sinkLeadTime >= c.fireDuration) { return true; }
    if (sinkStart < c.abandonTime) { return true; }
    if (!c.survivors.empty() && sinkStart < lastSurvivorTime(c)) { return true; }
    return false;
}

}
