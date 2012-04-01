1. INLEDNING

Wine �r ett program som g�r det m�jligt att k�ra Windows-program
(inkluderat DOS, Windows 3.x, Win32 och Win64) i Unix. Det best�r av en
programstartare som startar och k�r Windows-programfiler, samt ett
bibliotek (kallat Winelib) som implementerar Windows API-anrop med hj�lp
av deras Unix- eller X11-motsvarigheter. Biblioteket kan ocks� anv�ndas
till att porta Windows-kod till vanliga Unix-program.

Wine �r fri programvara, utgett under villkoren i GNU LGPL; se
filen LICENSE f�r detaljer.

2. KOM IG�NG

N�r du bygger Wine fr�n k�llkod s� rekommenderas du anv�nda Wines
installationsprogram. K�r f�ljande i rotkatalogen f�r Wines k�llkod:

./tools/wineinstall

K�r program med "wine [val] program". Se resten av denna fil,
Wines man-sidor samt sist men inte minst http://www.winehq.org/ f�r mer
information och tips om hur problem kan l�sas.


3. SYSTEMKRAV

F�r att kompilera och k�ra Wine kr�vs ett av f�ljande:

  Linux version 2.0.36 eller senare
  FreeBSD 7.0 eller senare
  Solaris x86 9 eller senare
  NetBSD-current
  Mac OS X 10.4 eller senare

Wine kr�ver st�d f�r tr�dar p� kernelniv�, och d�rf�r �r det bara
operativsystemen ovan som st�ds. Andra operativsystem som
st�der kerneltr�dar kommer eventuellt att st�djas i framtiden.

Information f�r FreeBSD
  Wine kommer i regel inte att fungera p� FreeBSD-versioner �ldre �n 7.0.
  Se http://wiki.freebsd.org/Wine f�r mer information.

Information f�r Solaris
  Wine m�ste antagligen byggas med GNU toolchain (gcc, gas etc.).
  Varning: �ven om gas installeras s� �r det inte s�kert att det anv�nds av
  gcc. Det s�gs att det �r n�dv�ndigt att antingen bygga gcc p� nytt, eller
  skapa symboliska l�nkar fr�n "cc", "as" och "ld" till GNU toolchain.

Information f�r NetBSD
  USER_LDT, SYSVSHM, SYSVSEM och SYSVMSG m�ste vara aktiverade i kerneln.

Information f�r Mac OS X:
  Du beh�ver Xcode 2.4 eller senare f�r att korrekt kunna bygga Wine p� x86.


St�dda filsystem
  Wine kan k�ra p� de flesta filsystem, men det har rapporterats problem vad
  g�ller kompatibilitet d� samba anv�nds f�r att ansluta till filer. NTFS
  tillhandah�ller inte heller alla filsystemsfunktioner som beh�vs av alla
  program. Det rekommenderas att anv�nda ett riktigt Unix-filsystem.

Grundl�ggande krav:
  Utvecklingsfilerna f�r X11 m�ste vara installerade (de kallas xlib6g-dev i
  Debian och XFree86-devel i Red Hat).

  Du m�ste givetvis ocks� ha "make" (mest troligt "GNU make").

  Det �r ocks� n�dv�ndigt att ha flex 2.5.33 eller senare samt bison.

Valfria st�dbibliotek:
  configure-skriptet visar varningar n�r valfria bibliotek inte hittats.
  Se http://wiki.winehq.org/Recommended_Packages f�r information om
  vilka paket du b�r installera.

  P� 64 bit-system m�ste du s�kerst�lla att 32 bit-versionerna av
  ovann�mnda bibliotek installerats; se http://wiki.winehq.org/WineOn64bit
  f�r n�rmare detaljer. Om du vill ha ett sant 64-bit Wine (eller en mixad
  32- och 64-bit Wine-installation), se http://wiki.winehq.org/Wine64 f�r
  detaljer.

4. KOMPILERING

K�r f�ljande kommandon f�r att bygga Wine om du inte anv�nder wineinstall:

./configure
make

Detta bygger programmet "wine" och diverse st�dbibliotek/programfiler.
Programfilen "wine" laddar och k�r Windows-program.
Biblioteket "libwine" ("Winelib") kan anv�ndas till att bygga och l�nka
Windows-k�llkod i Unix.

K�r './configure --help' f�r att se inst�llningar och val vid kompilering.

5. INSTALLATION

N�r Wine �r byggt kan du k�ra "make install" f�r att installera det;
detta installerar ocks� man-sidorna och n�gra fler n�dv�ndiga filer.

Gl�m inte att f�rst avinstallera gamla Wine-versioner. Pr�va antingen
"dpkg -r wine", "rpm -e wine" eller "make uninstall" f�re installationen.

N�r Wine �r installerat kan du anv�nda inst�llningsprogrammet "winecfg".
Se hj�lpavdelningen p� http://www.winehq.org/ f�r tips om inst�llningar.


6. K�RNING AV PROGRAM

N�r du anv�nder Wine kan du uppge hela s�kv�gen till programfilen, eller
enbart ett filnamn.

Exempel: f�r att k�ra Notepad:

        wine notepad               (anv�nder s�kv�gen angiven i Wines
        wine notepad.exe            konfigurationsfil f�r att finna filen)

        wine c:\\windows\\notepad.exe  (anv�ndning av DOS-filnamnssyntax)

        wine ~/.wine/drive_c/windows/notepad.exe  (anv�ndning av Unix-filv�gar)

        wine notepad.exe readme.txt  (k�ra program med parametrar)

Wine �r inte perfekt, s� det �r m�jligt att vissa program kraschar.
I s� fall f�r du en kraschlogg som du b�r bifoga till din rapport d� du
rapporterar ett fel.


7. MER INFORMATION

Internet:  Mycket information om Wine finns samlat p� WineHQ p�
           http://www.winehq.org/ : diverse guider, en programdatabas samt
           felsp�rning. Detta �r antagligen det b�sta st�llet att b�rja.

Fr�gor:    Fr�gor och svar om Wine finns samlade p� http://www.winehq.org/FAQ

Fel:       Rapportera fel till Wines Bugzilla p� http://bugs.winehq.org
           S�k i Bugzilla-databasen f�r att se om problemet redan finns
           rapporterat innan du s�nder en felrapport.

IRC:       Hj�lp finns tillg�nglig online p� kanalen #WineHQ p�
           irc.freenode.net.

Git:       Wines nuvarande utvecklingsversion finns tillg�nglig genom Git.
           G� till http://www.winehq.org/git f�r mer information.

E-postlistor:
           Det finns flera e-postlistor f�r Wine-anv�ndare och -utvecklare; se
           http://www.winehq.org/forums f�r mer information.

Wiki:      Wines Wiki finns p� http://wiki.winehq.org

Om du l�gger till n�got eller fixar ett fel, �r det bra om du s�nder
en patch (f�rslagsvis med git-format-patch) till listan wine-patches@winehq.org
f�r inkludering i n�sta utg�va av Wine.

--
Originalet till denna fil skrevs av
Alexandre Julliard
julliard@winehq.org

�versatt till svenska av
Anders Jonsson
anders.jonsson@norsjonet.se
