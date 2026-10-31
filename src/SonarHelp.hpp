/*   NAUTITECH - Simulateur de Navigation
     The sonar station's guide (AIDE key): how a 360 degree sonar and an echo sounder work, how to
     read them, and what the colours mean. French, the language of the station.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __SONARHELP_HPP_INCLUDED__
#define __SONARHELP_HPP_INCLUDED__

#include <string>
#include <vector>

namespace sonarhelp {

    //A heading, a paragraph or a point of a list
    struct Line { int kind; std::wstring text; };   //0 heading, 1 paragraph, 2 point
    enum { TAB_360 = 0, TAB_SOUNDER, TAB_COLOURS, TAB_COUNT };

    inline const wchar_t* tabName(int tab)
    {
        static const wchar_t* names[TAB_COUNT] = { L"SONAR 360\u00B0", L"SONDEUR", L"COULEURS" };
        return names[(tab >= 0 && tab < TAB_COUNT) ? tab : 0];
    }

    inline std::vector<Line> content(int tab)
    {
        std::vector<Line> l;
        auto H = [&](const wchar_t* t) { l.push_back(Line{ 0, t }); };
        auto P = [&](const wchar_t* t) { l.push_back(Line{ 1, t }); };
        auto B = [&](const wchar_t* t) { l.push_back(Line{ 2, t }); };

        if (tab == TAB_360) {
            H(L"Principe");
            P(L"Le sonar omnidirectionnel \u00E9met une impulsion acoustique sur 360\u00B0 autour du navire, dans un faisceau inclin\u00E9 vers le bas de l'angle d'inclinaison (tilt). L'image est renouvel\u00E9e \u00E0 chaque \u00E9mission, du centre vers l'ext\u00E9rieur, au fur et \u00E0 mesure du retour des \u00E9chos : le son se propage dans l'eau \u00E0 environ 1 500 m/s, soit 0,67 s pour une port\u00E9e de 500 m (aller et retour).");
            H(L"Lecture de l'\u00E9cran");
            B(L"Le navire est au centre (triangle) ; la ligne en tirets indique le cap.");
            B(L"CAP EN HAUT : l'avant du navire est en haut de l'\u00E9cran. NORD EN HAUT : le nord est en haut.");
            B(L"Les cercles sont les anneaux de distance, compt\u00E9e le long du faisceau (distance oblique). La graduation ext\u00E9rieure donne le rel\u00E8vement vrai.");
            H(L"L'\u00E9cho de fond");
            P(L"Le faisceau inclin\u00E9 rencontre le fond \u00E0 une distance qui d\u00E9pend de la profondeur et de l'inclinaison : distance oblique = profondeur \u00F7 sin(inclinaison), environ. Exemple : par 20 m de fond et 10\u00B0 d'inclinaison, le fond appara\u00EEt vers 115 m.");
            B(L"Le fond forme un anneau autour du navire. \u00C0 l'int\u00E9rieur de l'anneau : la colonne d'eau, o\u00F9 l'on recherche les bancs. Au-del\u00E0 : le fond arr\u00EAte le son, l'\u00E9cho s'\u00E9teint (zone d'ombre).");
            B(L"Anneau plus proche d'un c\u00F4t\u00E9 : le fond remonte de ce c\u00F4t\u00E9 (haut-fond, tombant).");
            B(L"Mur d'\u00E9chos forts suivi d'une zone sombre : c\u00F4te, banc de sable, roche \u00E9mergeante.");
            H(L"Choix de l'inclinaison");
            B(L"0 \u00E0 5\u00B0 : faisceau presque horizontal, grande port\u00E9e. Les \u00E9chos de surface (vagues, sillages) apparaissent, ainsi que les poissons proches du fond loin du navire.");
            B(L"10 \u00E0 20\u00B0 : r\u00E9glage de recherche habituel. Le fond n'occupe que la partie basse du faisceau ; les bancs en pleine eau apparaissent entre le navire et l'anneau de fond.");
            B(L"30 \u00E0 40\u00B0 : tout le faisceau touche le fond ; l'\u00E9cho de fond est complet et les poissons pos\u00E9s sur le fond y sont masqu\u00E9s.");
            B(L"Par petits fonds, si le fond envahit tout l'\u00E9cran : augmenter l'inclinaison, r\u00E9duire la port\u00E9e ou le gain.");
            H(L"Les cibles");
            B(L"Banc de poisson : tache arrondie ou ovale, verte \u00E0 rouge selon sa densit\u00E9, qui se d\u00E9place d'une \u00E9mission \u00E0 l'autre.");
            B(L"\u00C9pave, roche isol\u00E9e : \u00E9cho fort (orange, rouge) suivi d'une ombre acoustique, zone sombre derri\u00E8re la cible.");
            B(L"Autre navire : \u00E9cho fort pr\u00E8s de la surface ; son sillage laisse une tra\u00EEn\u00E9e faible de bulles.");
            B(L"Couche diffusante (plancton, petits poissons) : granulation faible et \u00E9tendue en pleine eau.");
            H(L"Marqueur et r\u00E9glages");
            B(L"Clic gauche sur l'image : marqueur, avec le rel\u00E8vement, la distance horizontale et la profondeur estim\u00E9e (distance oblique \u00D7 sin(inclinaison) + immersion du transducteur). Clic droit : effacer le marqueur.");
            B(L"Port\u00E9e : 100 m \u00E0 2 km ; la molette sur l'image la change aussi.");
            B(L"Gain : sensibilit\u00E9 du r\u00E9cepteur. R\u00E9gler jusqu'\u00E0 voir un l\u00E9ger bruit de fond, puis le diminuer un peu.");
            B(L"TVG (gain variable avec la distance) : au-dessus de 5, les \u00E9chos lointains sont renforc\u00E9s et les \u00E9chos proches (surface, eau autour du navire) att\u00E9nu\u00E9s ; au-dessous de 5, l'inverse. 5 : neutre.");
            B(L"Niveau \u00E9cho : au-dessus de 5, les \u00E9chos faibles sont affich\u00E9s plus forts ; au-dessous de 5, ils s'effacent et seuls les \u00E9chos forts restent. 5 : neutre.");
            B(L"Volume \u00E9cho : le haut-parleur du sonar. Chaque \u00E9mission fait un \u00AB ping \u00BB ; un \u00E9cho fort (banc, coque, \u00E9pave, fond) revient en \u00E9cho, d'autant plus fort que la cible est forte, au moment o\u00F9 le front de l'image l'atteint. Le haut-parleur \u00E9coute vers le marqueur s'il est pos\u00E9, sinon droit devant. 0 : coup\u00E9.");
            B(L"Compensation de houle : le faisceau est stabilis\u00E9, l'anneau de fond ne bouge pas avec le roulis et le pilonnement.");
        }
        else if (tab == TAB_SOUNDER) {
            H(L"Principe");
            P(L"Le sondeur \u00E9met verticalement, dans un c\u00F4ne sous le navire. Chaque \u00E9mission (ping) donne une colonne : la profondeur en ordonn\u00E9e, l'intensit\u00E9 de l'\u00E9cho en couleur. Les colonnes d\u00E9filent de droite \u00E0 gauche : le bord droit est l'instant pr\u00E9sent, l'historique est \u00E0 gauche.");
            P(L"Le chiffre en grand est la profondeur sous la surface, \u00E0 la verticale du navire.");
            H(L"L'\u00E9chogramme");
            B(L"Ligne z\u00E9ro : fine ligne en haut de l'\u00E9cran, l'instant d'\u00E9mission et la surface. Juste en dessous, les bulles : plus nombreuses par mer form\u00E9e et \u00E0 grande vitesse.");
            B(L"Fond dur (roche, gravier) : bord net et rouge, \u00E9cho fin, souvent un second \u00E9cho (\u00E9cho double) \u00E0 deux fois la profondeur.");
            B(L"Fond meuble (vase, sable fin) : \u00E9cho plus \u00E9pais et diffus qui s'estompe vers le bleu : le son p\u00E9n\u00E8tre dans le s\u00E9diment.");
            B(L"Poisson isol\u00E9 : trace en forme d'arche. En entrant dans le c\u00F4ne, le poisson est \u00E0 une distance oblique plus grande ; il est au plus pr\u00E8s \u00E0 la verticale, puis s'\u00E9loigne.");
            B(L"Banc : tache dense. Couche diffusante : bande granuleuse en pleine eau.");
            B(L"\u00C9pave : masse dress\u00E9e au-dessus du fond, au passage du navire.");
            B(L"Lignes verticales en pointill\u00E9s : changement d'\u00E9chelle.");
            B(L"A-scope (bande \u00E0 droite de l'image) : intensit\u00E9 de l'\u00E9cho de la derni\u00E8re \u00E9mission en fonction de la profondeur.");
            H(L"Les fr\u00E9quences");
            B(L"50 kHz (basse fr\u00E9quence) : c\u00F4ne large (environ 45\u00B0), port\u00E9e en profondeur importante, meilleure p\u00E9n\u00E9tration des fonds meubles, davantage de bruit ; les arches sont plus longues.");
            B(L"200 kHz (haute fr\u00E9quence) : c\u00F4ne \u00E9troit (environ 12\u00B0), meilleure r\u00E9solution et \u00E9chos plus fins, mais att\u00E9nuation plus forte avec la profondeur.");
            B(L"LES 2 : les deux \u00E9chogrammes c\u00F4te \u00E0 c\u00F4te, pour comparer.");
            H(L"R\u00E9glages");
            B(L"\u00C9chelle : profondeur affich\u00E9e. En automatique, le fond reste dans la partie basse de l'\u00E9cran.");
            B(L"Gain : sensibilit\u00E9. Anti-parasites : supprime les \u00E9chos les plus faibles (bruit, parasites).");
            B(L"TVG : au-dessus de 5, les \u00E9chos profonds sont renforc\u00E9s et ceux pr\u00E8s de la surface att\u00E9nu\u00E9s ; au-dessous de 5, l'inverse. 5 : neutre.");
            B(L"Niveau \u00E9cho : au-dessus de 5, les \u00E9chos faibles sont affich\u00E9s plus forts ; au-dessous de 5, ils s'effacent. 5 : neutre.");
            B(L"D\u00E9filement : vitesse d'avance de l'image.");
            B(L"Ligne blanche : le bord sup\u00E9rieur de l'\u00E9cho de fond est trac\u00E9 en blanc, pour distinguer les poissons pos\u00E9s sur le fond.");
            B(L"Compensation de houle : corrige le pilonnement du navire, comme un capteur de mouvements. Sans elle, le fond ondule avec la mer et la profondeur affich\u00E9e varie.");
        }
        else {
            H(L"Ce que montre la couleur");
            P(L"La couleur indique l'intensit\u00E9 de l'\u00E9cho, c'est-\u00E0-dire l'\u00E9nergie renvoy\u00E9e par la cible, et non sa nature. Une m\u00EAme cible change de couleur selon la distance, l'angle sous lequel le faisceau la frappe et le gain.");
            H(L"\u00C9chelle des couleurs (16 couleurs)");
            B(L"Couleur du fond d'\u00E9cran : aucun \u00E9cho, eau libre.");
            B(L"Bleu, bleu clair : \u00E9cho faible. Plancton, petits poissons dispers\u00E9s, fond meuble \u00E9loign\u00E9.");
            B(L"Vert, jaune : \u00E9cho moyen. Bancs de poisson, fond de sable ou de vase.");
            B(L"Orange, rouge : \u00E9cho fort. Banc dense, fond dur (roche), \u00E9pave, coque d'un navire.");
            B(L"Rouge fonc\u00E9, brun : \u00E9cho le plus fort. Roche, \u00E9pave, c\u00F4te.");
            H(L"Le gain");
            P(L"Le gain d\u00E9cale toute l'\u00E9chelle : un gain \u00E9lev\u00E9 fait passer des \u00E9chos faibles au jaune ou au rouge ; un gain faible fait dispara\u00EEtre les \u00E9chos faibles. Un banc qui reste rouge \u00E0 gain r\u00E9duit est dense.");
            H(L"Affichage");
            B(L"16 couleurs ou 8 couleurs : nombre de niveaux d'intensit\u00E9. La barre \u00AB faible / fort \u00BB du panneau montre les niveaux.");
            B(L"Ambre, Gris : palettes monochromes, plus reposantes la nuit.");
            B(L"Fond : bleu nuit, noir (de nuit), blanc (en plein jour), bleu clair.");
        }
        return l;
    }
}

#endif
