; ============================================================================
;  Simulateur de Navigation Maritime — NAUTITECH S.A.R.L.
;  Script d'installation Inno Setup (compiler avec Inno Setup 6.x)
;
;  RÉVISION : corrections du bug « le menu s'affiche mais les autres .exe
;  ne se lancent pas sur une machine cliente ».
;  Les 5 corrections sont marquées [CORRECTION 1] à [CORRECTION 5].
;  RÉVISION 2 : relecture contre le code source, marquée [RÉVISION 2] :
;  Simulator-fe.exe manquant, DLL inutiles (dont une DLL 32 bits), droits
;  d'écriture, raccourcis, dossiers doc et Scenarios.
;
;  ENCODAGE : enregistrez ce fichier en UTF-8 AVEC BOM (Notepad++ : Encodage >
;  UTF-8-BOM). Sans BOM, Inno Setup le lit en ANSI et les accents des
;  raccourcis (« Éditeur de scénarios ») s'affichent mal.
; ============================================================================
;
;  AVANT DE COMPILER :
;  1. Placez ces 3 fichiers dans le dossier "bin" (à côté des .exe) :
;        LICENSE.txt   README.txt   icon.ico
;  2. Vérifiez que Irrlicht.dll est bien la DLL active de votre build 64 bits
;     (lancez useVS64dll.bat une fois si nécessaire).
;  3. Ajustez #define SourceDir ci-dessous si votre chemin change.
;  4. Ouvrez ce fichier dans Inno Setup et cliquez sur "Compile" (F9).
;     L'installateur est généré dans le sous-dossier "Output".
;     (Models et World ne sont PAS lus à la compilation.)
;
;  POUR LIVRER / DISTRIBUER (clé USB, disque, dossier) :
;     Le dossier de livraison doit contenir, AU MÊME NIVEAU :
;        Setup_Simulateur_Navigation_v1.0.exe
;        Models\   (copié depuis votre bin)
;        World\    (copié depuis votre bin)
;     L'installateur copiera Models et World automatiquement à l'installation.
;     Si Models\ ou World\ manque à côté du Setup.exe, l'installation échouera.
;
; ----------------------------------------------------------------------------

#define MyAppName        "Simulateur de Navigation Maritime"
#define MyAppShortName   "Simulateur de Navigation"
#define MyAppVersion     "1.0"
#define MyAppPublisher   "NAUTITECH S.A.R.L."
#define MyAppExeName     "Simulator.exe"

; --- Dossier source contenant les fichiers du simulateur (votre bin) ---
#define SourceDir        "I:\CODING\Testing Version\bc-mainworking\bin"

; --- Motifs de fichiers de développement à ne jamais inclure ---
#define DevExcludes "*.cpp,*.hpp,*.h,*.rc,*.obj,*.o,*.pdb,*.exp,*.lib,*.ilk,*.iobj,*.ipdb,*.cbp,*.layout,*.depend,*.aps,*.db,*.tmp,*.log,Thumbs.db,desktop.ini"


[Setup]
AppId={{E756BDBF-2BC8-40F9-95B4-900DED4C8C8D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
; AppPublisherURL=https://www.nautitech.ma   ; (à compléter si souhaité)

; ---------------------------------------------------------------------------
; [CORRECTION 1] — CHEMIN D'INSTALLATION : C:\NAUTITECH\Simulateur
; [RÉVISION 2] Ce chemin convient, mais pas pour les raisons données avant :
;   - Réglages (bc5.ini), journaux, bilans, scénarios de l'utilisateur sont
;     écrits dans %APPDATA%\Bridge Command\<version>\, pas dans le dossier
;     d'installation. La licence ne fait que LIRE authorized_keys.txt.
;   - Les .exe sont lancés par leur nom (ShellExecute), sans souci d'espaces.
;   Seule exception : les outils « Taille et flottaison » et « Éclairage des
;   instruments » enregistrent dans Models\<navire>\boat.ini (voir [Dirs]).
; ---------------------------------------------------------------------------
DefaultDirName={sd}\NAUTITECH\Simulateur
DefaultGroupName={#MyAppShortName}
DisableProgramGroupPage=yes

; Contrat de licence affiché pendant l'installation
; Décommentez la ligne suivante UNIQUEMENT si bin\LICENSE.txt existe,
; sinon la compilation échouera.
; LicenseFile={#SourceDir}\LICENSE.txt

; Icône du programme d'installation et de désinstallation
SetupIconFile={#SourceDir}\icon.ico
UninstallDisplayIcon={app}\icon.ico
UninstallDisplayName={#MyAppName}

; ---------------------------------------------------------------------------
; [CORRECTION 2] — ARCHITECTURE
; "x64" est déprécié depuis Inno Setup 6.3 et provoque un avertissement de
; compilation. "x64compatible" est la valeur moderne équivalente.
; ---------------------------------------------------------------------------
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

; Sortie
OutputDir=I:\CODING\Testing Version\setup
OutputBaseFilename=Setup_Simulateur_Navigation_v{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

; Les dossiers volumineux Models et World NE SONT PAS intégrés au Setup.exe :
; ils sont copiés à l'installation depuis le dossier voisin (voir [Files],
; indicateur "external"). Le Setup.exe reste donc léger et en un seul fichier.
;
; Si la compilation signalait de nouveau un dépassement de taille (~4,2 Go),
; réactivez le découpage en retirant le " ; " des deux lignes ci-dessous :
; DiskSpanning=yes
; DiskSliceSize=max


[Languages]
Name: "french";  MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"


[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"


; ---------------------------------------------------------------------------
; [CORRECTION 3] — DOSSIERS INSCRIPTIBLES  (section [Dirs] absente auparavant)
;
; [RÉVISION 2] Plus de « users-modify » sur {app} lui-même : tout utilisateur
; pourrait sinon remplacer les .exe (lancés ensuite par l'instructeur ou un
; administrateur). Les programmes n'écrivent pas dans {app} ; seuls Models
; (boat.ini des outils de réglage) et Scenarios (éditeurs, quand le dossier
; utilisateur n'en a pas) doivent rester modifiables.
;
; Scenarios : sur VOTRE machine, bin\Scenarios existe (même vide). Sur la
; machine cliente il n'était créé NULLE PART, car la ligne [Files] qui le
; copie est désactivée. Simulator-ed.exe énumère ce dossier au démarrage :
; s'il est absent, l'éditeur se ferme immédiatement sans fenêtre.
; C'est la cause la plus probable de votre panne.
;
; Permissions : "users-modify" autorise un utilisateur standard (un élève,
; un instructeur sans droits admin) à écrire dans ces dossiers.
; ---------------------------------------------------------------------------
[Dirs]
Name: "{app}\Scenarios";    Permissions: users-modify
Name: "{app}\Models";       Permissions: users-modify


[Files]
; -------- Exécutables --------
Source: "{#SourceDir}\Simulator.exe";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\simulator-nav.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Simulator-mc.exe";  DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Simulator-rp.exe";  DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Simulator-mh.exe";  DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Simulator-ed.exe";  DestDir: "{app}"; Flags: ignoreversion
; [RÉVISION 2] Éditeur d'exercices incendie / SAR, lancé par le menu : il manquait.
Source: "{#SourceDir}\Simulator-fe.exe";  DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Simulator-ini.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\boat_pictures\*"; DestDir: "{app}\boat_pictures"; Excludes: "{#DevExcludes}"; Flags: ignoreversion recursesubdirs createallsubdirs

; -------- Bibliothèques d'exécution (DLL) --------
; [RÉVISION 2] Les .exe 64 bits n'importent que ces trois DLL (le reste vient
; de Windows ; ENet et la bibliothèque C++ sont liés statiquement, aucun
; « Visual C++ Redistributable » n'est nécessaire). Retirées :
;   libsndfile-1.dll  : DLL 32 bits, inutilisable par un .exe 64 bits ;
;   portaudio_x86.dll : 32 bits ;  libenet.dll : ENet est lié statiquement.
; Irrlicht.dll doit être la version 64 bits Visual Studio (useVS64dll.bat).
Source: "{#SourceDir}\Irrlicht.dll";      DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\sndfile.dll";       DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\portaudio_x64.dll"; DestDir: "{app}"; Flags: ignoreversion

; ---------------------------------------------------------------------------
; [CORRECTION 4] — DLL D'EXÉCUTION MinGW (filet de sécurité)
; Si un seul de vos .exe a été compilé avec MinGW plutôt qu'avec MSVC, il lui
; faut ces DLL. Elles sont dans votre PATH de développement, donc invisibles
; sur votre machine, et absentes chez le client. "skipifsourcedoesntexist"
; rend ces lignes inoffensives si les fichiers n'existent pas dans bin.
; ---------------------------------------------------------------------------
Source: "{#SourceDir}\libgcc_s_seh-1.dll";   DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\libgcc_s_dw2-1.dll";   DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\libstdc++-6.dll";      DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\libwinpthread-1.dll";  DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; -------- Configuration + fichiers de langue --------
; Ces .ini sont les réglages PAR DÉFAUT (chaque utilisateur garde les siens
; dans %APPDATA%). Vérifiez avant de compiler que bin\bc5.ini, mph.ini... ne
; contiennent pas vos réglages de développement (écran, ports, langue :
; mph.ini doit avoir lang="fr" pour un poste instructeur en français).
Source: "{#SourceDir}\*.ini";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\language*.txt"; DestDir: "{app}"; Flags: ignoreversion


; -------- Clés / licence (requis par le simulateur) --------
; Le motif "authorized_keys*" prend le fichier qu'il ait ou non l'extension .txt.
Source: "{#SourceDir}\authorized_keys*"; DestDir: "{app}"; Flags: ignoreversion

; -------- Documents --------
; Le raccourci « Lire le README » pointait vers un fichier jamais installé.
; Ces deux lignes le corrigent ; "skipifsourcedoesntexist" évite l'échec de
; compilation si les fichiers ne sont pas encore dans bin.
Source: "{#SourceDir}\README.txt";   DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\LICENSE.txt";  DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\icon.ico";     DestDir: "{app}"; Flags: ignoreversion

; -------- Données du simulateur (dossiers complets) --------
; NB : Models et World sont traités plus bas (external), pas ici.
; [RÉVISION 2] Scenarios et doc : "skipifsourcedoesntexist" permet de compiler
; même si le dossier est vide ou absent (le dossier Scenarios vide est créé
; par [Dirs]). Plus besoin de commenter / décommenter la ligne.
; doc\index.html est la page ouverte par le bouton d'aide du menu.
Source: "{#SourceDir}\Scenarios\*"; DestDir: "{app}\Scenarios"; Excludes: "{#DevExcludes}"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SourceDir}\doc\*";       DestDir: "{app}\doc";       Excludes: "{#DevExcludes}"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SourceDir}\media\*";     DestDir: "{app}\media";     Excludes: "{#DevExcludes}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\shaders\*";   DestDir: "{app}\shaders";   Excludes: "{#DevExcludes}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\Sounds\*";    DestDir: "{app}\Sounds";    Excludes: "{#DevExcludes}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\Scripts\*";   DestDir: "{app}\Scripts";   Excludes: "{#DevExcludes}"; Flags: ignoreversion recursesubdirs createallsubdirs

; -------- Models et World : NON intégrés au Setup.exe (external) --------
; Copiés PENDANT l'installation depuis le dossier qui contient le Setup.exe.
;   => Sur la clé USB / le support de livraison, placez les dossiers "Models"
;      et "World" JUSTE À CÔTÉ du fichier Setup.exe (au même niveau).
; À la compilation ces fichiers ne sont PAS lus : Setup.exe reste léger et la
; compilation plus rapide. Ils DOIVENT être présents au moment de l'installation.
Source: "{src}\Models\*"; DestDir: "{app}\Models"; Flags: external recursesubdirs createallsubdirs
Source: "{src}\World\*";  DestDir: "{app}\World";  Flags: external recursesubdirs createallsubdirs


[UninstallDelete]
; Les fichiers "external" (Models/World) sont supprimés à la désinstallation ;
; ces lignes garantissent aussi le retrait des dossiers eux-mêmes.
Type: filesandordirs; Name: "{app}\Models"
Type: filesandordirs; Name: "{app}\World"


; ---------------------------------------------------------------------------
; [CORRECTION 5] — WorkingDir SUR TOUS LES RACCOURCIS  (LA CORRECTION « CAUSE A »)
;
; Sans WorkingDir, Windows lance le programme avec un répertoire courant
; arbitraire. Tous les chemins RELATIFS du simulateur (media\, shaders\,
; Scenarios\, et surtout le chemin vers les .exe enfants) sont alors résolus
; au mauvais endroit. Sur votre machine vous lancez depuis le dossier bin,
; donc le répertoire courant est toujours correct et le bug reste invisible.
;
; NOTE : WorkingDir doit être sur LA MÊME LIGNE que Name/Filename, séparé par
; un point-virgule. Sur une ligne à part, Inno le lit comme une entrée
; distincte et refuse de compiler.
; ---------------------------------------------------------------------------
; [RÉVISION 2] Le poste instructeur multijoueur est Simulator-mh.exe (le hub) ;
; Simulator-mc.exe est le contrôleur de carte. Ajout de l'éditeur incendie/SAR.
; Le raccourci README n'est créé que si le fichier a été installé.
[Icons]
Name: "{group}\{#MyAppShortName}";                       Filename: "{app}\{#MyAppExeName}";   WorkingDir: "{app}"; IconFilename: "{app}\icon.ico"
Name: "{group}\Poste instructeur (multijoueur)";         Filename: "{app}\Simulator-mh.exe";  WorkingDir: "{app}"; IconFilename: "{app}\icon.ico"
Name: "{group}\Contrôleur de carte";                     Filename: "{app}\Simulator-mc.exe";  WorkingDir: "{app}"; IconFilename: "{app}\icon.ico"
Name: "{group}\Éditeur de scénarios";                    Filename: "{app}\Simulator-ed.exe";  WorkingDir: "{app}"; IconFilename: "{app}\icon.ico"
Name: "{group}\Éditeur incendie - SAR";                  Filename: "{app}\Simulator-fe.exe";  WorkingDir: "{app}"; IconFilename: "{app}\icon.ico"
Name: "{group}\Configuration";                           Filename: "{app}\Simulator-ini.exe"; WorkingDir: "{app}"; IconFilename: "{app}\icon.ico"
Name: "{group}\Lire le README";                          Filename: "{app}\README.txt";        WorkingDir: "{app}"; Check: FileExists(ExpandConstant('{app}\README.txt'))
Name: "{group}\{cm:UninstallProgram,{#MyAppShortName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppShortName}";                 Filename: "{app}\{#MyAppExeName}";   WorkingDir: "{app}"; IconFilename: "{app}\icon.ico"; Tasks: desktopicon


[Run]
Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,{#MyAppShortName}}"; Flags: nowait postinstall skipifsilent
