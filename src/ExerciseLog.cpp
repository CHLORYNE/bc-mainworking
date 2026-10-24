/*   NAUTITECH - Simulateur de Navigation
     Exercise record for the debrief. See ExerciseLog.hpp.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "ExerciseLog.hpp"

#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

//Time conversions: Visual Studio refuses localtime/gmtime (C4996), so its _s versions are used there
static void toLocalTime(time_t t, struct tm& out)
{
#ifdef _MSC_VER
    localtime_s(&out, &t);
#else
    const struct tm* p = localtime(&t);
    if (p) { out = *p; } else { out = tm(); }
#endif
}

static void toUtcTime(time_t t, struct tm& out)
{
#ifdef _MSC_VER
    gmtime_s(&out, &t);
#else
    const struct tm* p = gmtime(&t);
    if (p) { out = *p; } else { out = tm(); }
#endif
}

namespace {

const float TRACK_INTERVAL_S = 10.0f;

std::string utf8(const std::wstring& w)
{
    std::string out;
    for (size_t i = 0; i < w.size(); i++) {
        unsigned int c = (unsigned int)w[i];
        if (c < 0x80) { out += (char)c; }
        else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 0x3F)); }
        else { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
    }
    return out;
}

std::string html(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        switch (s[i]) {
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '&': out += "&amp;"; break;
        case '"': out += "&quot;"; break;
        default: out += s[i];
        }
    }
    return out;
}

std::string clock(float seconds)
{
    if (seconds < 0) { seconds = 0; }
    const int t = (int)(seconds + 0.5f);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60);
    return buf;
}

std::string position(float lat, float lon)
{
    char buf[64];
    const float alat = std::fabs(lat), alon = std::fabs(lon);
    snprintf(buf, sizeof(buf), "%02d\xC2\xB0%06.3f'%c %03d\xC2\xB0%06.3f'%c",
        (int)alat, (alat - (int)alat) * 60.0f, lat >= 0 ? 'N' : 'S',
        (int)alon, (alon - (int)alon) * 60.0f, lon >= 0 ? 'E' : 'W');
    return buf;
}

std::string fixed(float v, int decimals)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

//Distance in nautical miles between two positions (flat over the short legs used here)
double nmBetween(float lat1, float lon1, float lat2, float lon2)
{
    const double midLat = 0.5 * (lat1 + lat2) * 3.14159265358979 / 180.0;
    const double dy = (lat2 - lat1) * 60.0;
    const double dx = (lon2 - lon1) * 60.0 * std::cos(midLat);
    return std::sqrt(dx * dx + dy * dy);
}

void makeFolder(const std::string& path)
{
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}

std::string safeName(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        const char c = s[i];
        out += (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') ? '_' : c;
    }
    return out;
}

const char* categoryName(ExerciseLog::Category c)
{
    switch (c) {
    case ExerciseLog::EV_COLLISION: return "Abordage";
    case ExerciseLog::EV_GROUNDING: return "\xC3\x89" "chouement";
    case ExerciseLog::EV_CONTACT: return "Contact";
    case ExerciseLog::EV_ALARM: return "Alarme";
    case ExerciseLog::EV_SIGNAL: return "Signal sonore";
    case ExerciseLog::EV_FAILURE: return "Avarie";
    case ExerciseLog::EV_INSTRUCTOR: return "Instructeur";
    default: return "Information";
    }
}

const char* categoryColour(ExerciseLog::Category c)
{
    switch (c) {
    case ExerciseLog::EV_COLLISION: case ExerciseLog::EV_GROUNDING: return "#d93025";
    case ExerciseLog::EV_CONTACT: case ExerciseLog::EV_ALARM: return "#e8710a";
    case ExerciseLog::EV_SIGNAL: return "#1a73e8";
    case ExerciseLog::EV_FAILURE: case ExerciseLog::EV_INSTRUCTOR: return "#8e44ad";
    default: return "#5f6368";
    }
}

} // namespace

void ExerciseLog::begin(const std::string& scenario, const std::string& world, const std::string& ownShip, uint64_t startTimestamp)
{
    *this = ExerciseLog();
    started = true;
    scenarioName = scenario;
    worldName = world;
    ownShipName = ownShip;
    startTime = startTimestamp;
}

void ExerciseLog::update(float exerciseSeconds, const OwnState& own, const std::vector<OtherState>& others)
{
    if (!started) { return; }
    if (!track.empty() || lastSample > -1000) {
        const double d = nmBetween(lastOwn.lat, lastOwn.lon, own.lat, own.lon);
        if (d < 1.0) { distanceNm += d; } //(not a jump of the position by the instructor)
    }
    lastOwn = own;
    lastSeconds = exerciseSeconds;
    if (own.sogKts > maxSogKts) { maxSogKts = own.sogKts; }

    if (otherTracks.size() < others.size()) { otherTracks.resize(others.size()); }
    for (size_t i = 0; i < others.size(); i++) {
        OtherTrack& o = otherTracks[i];
        o.name = others[i].name;
        if (!others[i].present) { continue; }
        if (o.minRangeNm < 0 || others[i].rangeNm < o.minRangeNm) {
            o.minRangeNm = others[i].rangeNm;
            o.minRangeTime = exerciseSeconds;
            o.minRangeLat = own.lat;
            o.minRangeLon = own.lon;
        }
    }

    if (exerciseSeconds - lastSample >= TRACK_INTERVAL_S) {
        lastSample = exerciseSeconds;
        TrackPoint p;
        p.t = exerciseSeconds;
        p.s = own;
        track.push_back(p);
        for (size_t i = 0; i < others.size(); i++) {
            if (!others[i].present) { continue; }
            otherTracks[i].lat.push_back(others[i].lat);
            otherTracks[i].lon.push_back(others[i].lon);
        }
    }
}

void ExerciseLog::event(float exerciseSeconds, Category category, const std::wstring& text)
{
    if (!started) { return; }
    Event e;
    e.t = exerciseSeconds;
    e.category = category;
    e.text = text;
    e.lat = lastOwn.lat;
    e.lon = lastOwn.lon;
    events.push_back(e);
}

int ExerciseLog::count(Category category) const
{
    int n = 0;
    for (size_t i = 0; i < events.size(); i++) { if (events[i].category == category) { n++; } }
    return n;
}

float ExerciseLog::closestApproachNm(std::string* shipName) const
{
    float best = -1;
    for (size_t i = 0; i < otherTracks.size(); i++) {
        if (otherTracks[i].minRangeNm >= 0 && (best < 0 || otherTracks[i].minRangeNm < best)) {
            best = otherTracks[i].minRangeNm;
            if (shipName) { *shipName = otherTracks[i].name; }
        }
    }
    return best;
}

bool ExerciseLog::write(const std::string& folder, std::string& path) const
{
    if (!started || track.empty()) { return false; }
    makeFolder(folder);

    //File name: date and time of the end of the exercise, then the exercise
    char stamp[32];
    const time_t now = time(0);
    struct tm lt;
    toLocalTime(now, lt);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d_%Hh%M", &lt);
    const std::string base = folder + "/" + stamp + "_" + safeName(scenarioName);
    path = base + ".html";

    //Track as CSV (opens in a spreadsheet)
    {
        std::ofstream csv((base + "_trace.csv").c_str());
        if (csv) {
            csv << "temps;latitude;longitude;cap;route_fond;vitesse_fond_nd;vitesse_surface_nd;barre;machine_bd;machine_td;profondeur_m\n";
            for (size_t i = 0; i < track.size(); i++) {
                const OwnState& s = track[i].s;
                csv << clock(track[i].t) << ";" << fixed(s.lat, 6) << ";" << fixed(s.lon, 6) << ";" << fixed(s.heading, 1) << ";"
                    << fixed(s.cog, 1) << ";" << fixed(s.sogKts, 1) << ";" << fixed(s.stwKts, 1) << ";" << fixed(s.rudder, 1) << ";"
                    << fixed(s.portEngine * 100, 0) << ";" << fixed(s.stbdEngine * 100, 0) << ";" << fixed(s.depth, 1) << "\n";
            }
        }
    }

    std::ofstream f(path.c_str());
    if (!f) { return false; }

    //Plot: everything in nautical miles from the start position (flat over the exercise area)
    const float lat0 = track[0].s.lat, lon0 = track[0].s.lon;
    const double cosLat = std::cos(lat0 * 3.14159265358979 / 180.0);
    auto toX = [&](float lon) { return (lon - lon0) * 60.0 * cosLat; };
    auto toY = [&](float lat) { return (lat - lat0) * 60.0; };
    double minX = 0, maxX = 0, minY = 0, maxY = 0;
    auto extend = [&](float lat, float lon) {
        minX = std::min(minX, toX(lon)); maxX = std::max(maxX, toX(lon));
        minY = std::min(minY, toY(lat)); maxY = std::max(maxY, toY(lat));
    };
    for (size_t i = 0; i < track.size(); i++) { extend(track[i].s.lat, track[i].s.lon); }
    for (size_t i = 0; i < otherTracks.size(); i++) {
        for (size_t k = 0; k < otherTracks[i].lat.size(); k++) { extend(otherTracks[i].lat[k], otherTracks[i].lon[k]); }
    }
    double spanX = std::max(maxX - minX, 0.2), spanY = std::max(maxY - minY, 0.2);
    const double pad = 0.08 * std::max(spanX, spanY);
    minX -= pad; maxX = minX + spanX + 2 * pad; minY -= pad; maxY = minY + spanY + 2 * pad;
    spanX = maxX - minX; spanY = maxY - minY;
    const double W = 900, H = std::max(300.0, std::min(900.0, W * spanY / spanX));
    const double scale = std::min(W / spanX, H / spanY);
    auto px = [&](float lon) { return (toX(lon) - minX) * scale; };
    auto py = [&](float lat) { return H - (toY(lat) - minY) * scale; };

    //Summary figures
    std::string closestName;
    const float closest = closestApproachNm(&closestName);
    float duration = lastSeconds - track[0].t;
    double meanSog = 0;
    for (size_t i = 0; i < track.size(); i++) { meanSog += track[i].s.sogKts; }
    meanSog /= track.size();
    float minDepth = 1e9f;
    for (size_t i = 0; i < track.size(); i++) { if (track[i].s.depth > 0 && track[i].s.depth < minDepth) { minDepth = track[i].s.depth; } }

    char startText[32];
    const time_t st = (time_t)startTime;
    struct tm gt;
    toUtcTime(st, gt);
    strftime(startText, sizeof(startText), "%d/%m/%Y %H:%M", &gt);

    f << "<!doctype html>\n<html lang=\"fr\"><head><meta charset=\"utf-8\">\n"
      << "<title>Bilan - " << html(scenarioName) << "</title>\n"
      << "<style>\n"
      << "body{font-family:Segoe UI,Arial,sans-serif;margin:24px;color:#202124;background:#fff}\n"
      << "h1{margin:0 0 4px;font-size:24px}h2{font-size:18px;margin:28px 0 10px;border-bottom:2px solid #e8eaed;padding-bottom:4px}\n"
      << ".sub{color:#5f6368;margin-bottom:18px}.cards{display:flex;flex-wrap:wrap;gap:12px}\n"
      << ".card{border:1px solid #dadce0;border-radius:8px;padding:10px 14px;min-width:150px}\n"
      << ".card b{display:block;font-size:22px;margin-top:2px}.card.bad b{color:#d93025}.card.ok b{color:#188038}\n"
      << "table{border-collapse:collapse;width:100%;font-size:14px}th,td{border-bottom:1px solid #e8eaed;padding:6px 8px;text-align:left}\n"
      << "th{background:#f8f9fa}.tag{display:inline-block;padding:1px 8px;border-radius:10px;color:#fff;font-size:12px}\n"
      << "svg{border:1px solid #dadce0;border-radius:8px;background:#f4f8fb;max-width:100%;height:auto}\n"
      << "@media print{body{margin:8mm}}\n"
      << "</style></head><body>\n";
    f << "<h1>Bilan d'exercice : " << html(scenarioName) << "</h1>\n";
    f << "<div class=\"sub\">Navire : " << html(ownShipName) << " &middot; Zone : " << html(worldName)
      << " &middot; D\xC3\xA9" "but de l'exercice (heure du sc\xC3\xA9nario) : " << startText << " UTC &middot; Dur\xC3\xA9" "e : " << clock(duration) << "</div>\n";

    const int collisions = count(EV_COLLISION), groundings = count(EV_GROUNDING), contacts = count(EV_CONTACT), alarms = count(EV_ALARM);
    f << "<div class=\"cards\">\n";
    f << "<div class=\"card\">Distance parcourue<b>" << fixed((float)distanceNm, 2) << " NM</b></div>\n";
    f << "<div class=\"card\">Vitesse moyenne / max<b>" << fixed((float)meanSog, 1) << " / " << fixed(maxSogKts, 1) << " nd</b></div>\n";
    if (closest >= 0) {
        f << "<div class=\"card " << (closest < 0.5f ? "bad" : "ok") << "\">Plus petite distance \xC3\xA0 un navire<b>"
          << fixed(closest, 2) << " NM</b>" << html(closestName) << "</div>\n";
    }
    f << "<div class=\"card " << (collisions ? "bad" : "ok") << "\">Abordages<b>" << collisions << "</b></div>\n";
    f << "<div class=\"card " << (groundings ? "bad" : "ok") << "\">\xC3\x89" "chouements<b>" << groundings << "</b></div>\n";
    f << "<div class=\"card\">Contacts (quai, bou\xC3\xA9" "e)<b>" << contacts << "</b></div>\n";
    f << "<div class=\"card\">Alarmes<b>" << alarms << "</b></div>\n";
    if (minDepth < 1e8f) { f << "<div class=\"card\">Profondeur minimale<b>" << fixed(minDepth, 1) << " m</b></div>\n"; }
    f << "</div>\n";

    //Track plot
    f << "<h2>Trac\xC3\xA9</h2>\n";
    f << "<svg viewBox=\"0 0 " << (int)W << " " << (int)H << "\" width=\"" << (int)W << "\" height=\"" << (int)H << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    //Grid every whole number of a nice step
    {
        const double steps[] = { 0.1, 0.2, 0.5, 1, 2, 5, 10, 20 };
        double step = steps[0];
        for (int i = 0; i < 8; i++) { step = steps[i]; if (std::max(spanX, spanY) / step <= 10) { break; } }
        for (double x = std::ceil(minX / step) * step; x <= maxX; x += step) {
            const double sx = (x - minX) * scale;
            f << "<line x1=\"" << sx << "\" y1=\"0\" x2=\"" << sx << "\" y2=\"" << H << "\" stroke=\"#dfe6ec\"/>\n";
        }
        for (double y = std::ceil(minY / step) * step; y <= maxY; y += step) {
            const double sy = H - (y - minY) * scale;
            f << "<line x1=\"0\" y1=\"" << sy << "\" x2=\"" << W << "\" y2=\"" << sy << "\" stroke=\"#dfe6ec\"/>\n";
        }
        //Scale bar and north arrow
        const double bar = step * scale;
        f << "<line x1=\"20\" y1=\"" << H - 20 << "\" x2=\"" << 20 + bar << "\" y2=\"" << H - 20 << "\" stroke=\"#202124\" stroke-width=\"3\"/>\n"
          << "<text x=\"20\" y=\"" << H - 28 << "\" font-size=\"13\">" << fixed((float)step, step < 1 ? 1 : 0) << " NM</text>\n"
          << "<path d=\"M" << W - 30 << " 50 L" << W - 22 << " 70 L" << W - 38 << " 70 Z\" fill=\"#202124\"/>"
          << "<text x=\"" << W - 35 << " \" y=\"44\" font-size=\"14\" font-weight=\"bold\">N</text>\n";
    }
    //Other ships: thin lines, name at the last position
    for (size_t i = 0; i < otherTracks.size(); i++) {
        const OtherTrack& o = otherTracks[i];
        if (o.lat.empty()) { continue; }
        f << "<polyline fill=\"none\" stroke=\"#80868b\" stroke-width=\"1.5\" stroke-dasharray=\"4 3\" points=\"";
        for (size_t k = 0; k < o.lat.size(); k++) { f << px(o.lon[k]) << "," << py(o.lat[k]) << " "; }
        f << "\"/>\n<circle cx=\"" << px(o.lon.back()) << "\" cy=\"" << py(o.lat.back()) << "\" r=\"4\" fill=\"#80868b\"/>"
          << "<text x=\"" << px(o.lon.back()) + 6 << "\" y=\"" << py(o.lat.back()) - 6 << "\" font-size=\"12\" fill=\"#5f6368\">" << html(o.name) << "</text>\n";
    }
    //Own ship: bold, start and end marks, a time label every 5 minutes
    f << "<polyline fill=\"none\" stroke=\"#1a73e8\" stroke-width=\"3\" points=\"";
    for (size_t i = 0; i < track.size(); i++) { f << px(track[i].s.lon) << "," << py(track[i].s.lat) << " "; }
    f << "\"/>\n";
    float nextLabel = track[0].t + 300;
    for (size_t i = 1; i < track.size(); i++) {
        if (track[i].t >= nextLabel) {
            nextLabel += 300;
            f << "<circle cx=\"" << px(track[i].s.lon) << "\" cy=\"" << py(track[i].s.lat) << "\" r=\"3\" fill=\"#1a73e8\"/>"
              << "<text x=\"" << px(track[i].s.lon) + 5 << "\" y=\"" << py(track[i].s.lat) + 14 << "\" font-size=\"11\" fill=\"#1a73e8\">"
              << clock(track[i].t).substr(0, 5) << "</text>\n";
        }
    }
    f << "<circle cx=\"" << px(track[0].s.lon) << "\" cy=\"" << py(track[0].s.lat) << "\" r=\"6\" fill=\"#188038\"/>"
      << "<text x=\"" << px(track[0].s.lon) + 8 << "\" y=\"" << py(track[0].s.lat) - 8 << "\" font-size=\"13\" fill=\"#188038\">D\xC3\xA9part</text>\n";
    f << "<circle cx=\"" << px(lastOwn.lon) << "\" cy=\"" << py(lastOwn.lat) << "\" r=\"6\" fill=\"#1a73e8\"/>"
      << "<text x=\"" << px(lastOwn.lon) + 8 << "\" y=\"" << py(lastOwn.lat) - 8 << "\" font-size=\"13\" fill=\"#1a73e8\">Fin</text>\n";
    //Events
    for (size_t i = 0; i < events.size(); i++) {
        const Event& e = events[i];
        if (e.category == EV_INFO || e.category == EV_SIGNAL) { continue; }
        f << "<g><title>" << clock(e.t) << " " << html(utf8(e.text)) << "</title><circle cx=\"" << px(e.lon) << "\" cy=\"" << py(e.lat)
          << "\" r=\"7\" fill=\"none\" stroke=\"" << categoryColour(e.category) << "\" stroke-width=\"3\"/></g>\n";
    }
    f << "</svg>\n<div class=\"sub\">Trait bleu : votre navire (rep\xC3\xA8res toutes les 5 minutes). Pointill\xC3\xA9s gris : les autres navires. "
      << "Cercles de couleur : les \xC3\xA9v\xC3\xA9nements (passer la souris dessus).</div>\n";

    //Closest approach to each ship
    bool anyShip = false;
    for (size_t i = 0; i < otherTracks.size(); i++) { if (otherTracks[i].minRangeNm >= 0) { anyShip = true; } }
    if (anyShip) {
        f << "<h2>Distance minimale \xC3\xA0 chaque navire</h2>\n<table><tr><th>Navire</th><th>Distance minimale</th><th>\xC3\x80</th><th>Position (votre navire)</th></tr>\n";
        for (size_t i = 0; i < otherTracks.size(); i++) {
            const OtherTrack& o = otherTracks[i];
            if (o.minRangeNm < 0) { continue; }
            f << "<tr><td>" << html(o.name) << "</td><td" << (o.minRangeNm < 0.5f ? " style=\"color:#d93025;font-weight:bold\"" : "") << ">"
              << fixed(o.minRangeNm, 2) << " NM (" << (int)(o.minRangeNm * 1852.0f + 0.5f) << " m)</td><td>" << clock(o.minRangeTime)
              << "</td><td>" << position(o.minRangeLat, o.minRangeLon) << "</td></tr>\n";
        }
        f << "</table>\n";
    }

    //Events
    f << "<h2>Journal des \xC3\xA9v\xC3\xA9nements</h2>\n";
    if (events.empty()) {
        f << "<p>Aucun \xC3\xA9v\xC3\xA9nement.</p>\n";
    }
    else {
        f << "<table><tr><th>Temps</th><th>Type</th><th>\xC3\x89v\xC3\xA9nement</th><th>Position</th></tr>\n";
        for (size_t i = 0; i < events.size(); i++) {
            const Event& e = events[i];
            f << "<tr><td>" << clock(e.t) << "</td><td><span class=\"tag\" style=\"background:" << categoryColour(e.category) << "\">"
              << categoryName(e.category) << "</span></td><td>" << html(utf8(e.text)) << "</td><td>" << position(e.lat, e.lon) << "</td></tr>\n";
        }
        f << "</table>\n";
    }
    f << "<p class=\"sub\">Le trac\xC3\xA9 d\xC3\xA9taill\xC3\xA9 (toutes les 10 s) est dans le fichier _trace.csv \xC3\xA0 c\xC3\xB4t\xC3\xA9 de ce bilan.</p>\n";
    f << "</body></html>\n";
    return true;
}
