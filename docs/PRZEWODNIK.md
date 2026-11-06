# OpenLocoland - przewodnik gracza

Wersja 0.2.9; edytor map jest wciąż w budowie. [English version: GUIDE.md](GUIDE.md)

**OpenLocoland** to nieoficjalna, fanowska nowa edycja gry **Locoland** (Gromada / ESDigital
Games, 2003): oryginalna gra działa z poprawkami dla dzisiejszego Windowsa, kilkoma ulepszeniami,
stroną własnych map w menu i **edytorem map** wbudowanym w grę. Projekt nie jest związany z
właścicielami praw do Locolandu.

Paczka **nie zawiera** gry. Potrzebny jest twój własny **Locoland ze Steama**: instalator bierze
z niego pliki gry i składa OpenLocoland w osobnym folderze. Instalacja Steama się nie zmienia -
oryginał nadal uruchomisz ze Steama.

Spis treści:

1. [Instalacja, aktualizacja, odinstalowanie](#1-instalacja-aktualizacja-odinstalowanie)
2. [Pierwsze uruchomienie](#2-pierwsze-uruchomienie)
3. [Co nowego w grze](#3-co-nowego-w-grze)
4. [Własne mapy i POLIGON](#4-własne-mapy-i-poligon)
5. [Edytor map](#5-edytor-map)
6. [Samouczek: twoja pierwsza misja](#6-samouczek-twoja-pierwsza-misja)
7. [Klawisze i narzędzia edytora](#7-klawisze-i-narzędzia-edytora)
8. [Pytania i problemy](#8-pytania-i-problemy)

## 1. Instalacja, aktualizacja, odinstalowanie

Potrzebne są: Windows 10 lub 11 i Locoland zainstalowany przez Steam (w dowolnym języku).

**Instalacja.** Uruchom `Setup.exe`. Instalator znajdzie twojego Locolanda ze Steama i
zainstaluje OpenLocoland w `C:\Users\<ty>\Games\OpenLocoland` (można wybrać inny folder). Tworzy skróty:
- **Locoland** - gra,
- **Locoland - edytor map** - edytor na pustej mapie,

na pulpicie i w menu Start (instalator może ich nie tworzyć).

**Język gry.** Lista w instalatorze pokazuje wszystkie języki gry. Wybrać można języki twojej kopii
ze Steama oraz fanowskie tłumaczenia oznaczone **(nieoficjalne)** (czeski), które instalator
zbuduje na dowolnym języku, jaki masz. Jeśli masz czeskie wydanie gry (płyta „Steamland”), umieść
jego folder o nazwie `Steamland` obok folderu gry ze Steama - albo wskaż go jako folder Steama, gdy
nie masz kopii ze Steama: instalator użyje wtedy jego własnych tekstów i czcionek dla czeskiego. Pozostałe są wyszarzone, a obok jest wiersz, który pobiera
je ze Steama (np. `download_depot 352130 352132` dla niemieckiego): wpisz go w konsoli Steama
(`steam://open/console` w pasku adresu przeglądarki albo w Win+R), poczekaj, aż Steam go pobierze,
i uruchom instalator ponownie - zostawi wybrany język i doda nowy.

![Lista języków w instalatorze](img/setup-languages-pl.jpg)

Folder gry jest samodzielny: są w nim twoje mapy, tabele wyników i logi. Profil gracza (imię,
wygrane misje, wyniki, ustawienia dźwięku) jest zapisany w rejestrze twojego użytkownika Windows,
tak jak w oryginale.

**Aktualizacja.** Uruchom nowszy `Setup.exe` i wciśnij **Aktualizuj**: znajdzie zainstalowaną grę i skopiuje tylko to, co się zmieniło. Twoje mapy (w `maps\` folderu gry) i twój profil zostają.

Czy jest nowsza wersja, sprawdzisz przyciskiem **Sprawdź aktualizacje** w okienku startowym (zob.
[pierwsze uruchomienie](#2-pierwsze-uruchomienie)). Okienko pyta stronę projektu na GitHubie i
odpowiada „Masz aktualną wersję” albo „Jest nowa wersja ...”; wtedy **Pobierz** zapisuje nowy
`Setup.exe` w folderze Pobrane, a **Uruchom Setup** uruchamia go (po pytaniu) i zamyka grę, żeby
instalator mógł ją zaktualizować - Windows może zapytać, czy uruchomić pobrany program, jak po
pobraniu przeglądarką. Link **GitHub** otwiera zamiast tego stronę wydań w przeglądarce (prawy
przycisk myszy kopiuje adres). **przy starcie** jest domyślnie włączone: okienko sprawdza przy
otwarciu, najwyżej raz dziennie i bez żadnego okna - tylko w swojej linijce, która bez połączenia
spokojnie mówi, że sprawdzanie pominięto; odznacz pole obok przycisku, żeby je wyłączyć (zapisuje się
od razu). Nic nie jest pobierane ani instalowane bez twojego kliknięcia. Gdy przycisk nie może
dosięgnąć GitHuba (brak internetu albo projekt się przeniósł), okienko mówi o tym i
podaje adres projektu z pliku `update.txt` w folderze gry - późniejszy Setup może ten plik zmienić,
ty też.

**Odinstalowanie.** Uruchom `Setup.exe` i wciśnij **Odinstaluj**: usuwa skróty i folder gry ze wszystkim, co w nim jest (OpenLocoland nie figuruje w „Aplikacjach” Windows). Gra ze Steama zostaje bez zmian. Jeśli chcesz zachować swoje
mapy, skopiuj je najpierw (każda to folder w `maps\`, zob. [udostępnianie mapy](#13-zapis-eksport-i-udostępnianie)).

## 2. Pierwsze uruchomienie

Gra startuje okienkiem startowym (z obrazkiem Locolandu):

![Okienko startowe](img/launcher-update-pl.jpg)

- **Rozdzielczość** - oprócz 640 x 480 ... 1024 x 768 lista ma tryby panoramiczne twojego monitora
  (od 1280 x 720 do 3840 x 2160) i rozdzielczość pulpitu.
- **Pełny ekran** - pole pod obrazkiem. Gdy nie jest zaznaczone, gra działa w **oknie** (okno
  pamięta swoje miejsce).
- **Język** - lista obok: języki, które masz (polski, angielski, ...; fanowskie tłumaczenie jest
  oznaczone „(nieoficjalne)”). Napisy okienka zmieniają się razem z nim. Jeśli wybranego języka już
  nie ma, gra startuje po angielsku, a bez angielskiego w języku twojej kopii ze Steama.
- **Licznik FPS** i **Limit FPS** - wiersz pod rozdzielczością: mały licznik u góry ekranu i ile
  klatek na sekundę rysuje gra (domyślnie 60; ∞ = bez limitu).
- **Kody twórców** i **Wszystkie misje** - dwa dodatki, domyślnie wyłączone: klawisze twórców gry i
  wszystkie misje otwarte od początku (prawdziwe postępy zostają nietknięte).
- **Sprawdź aktualizacje** - wiersz nad OK: czy jest nowsza wersja OpenLocoland, zob.
  [aktualizacja](#1-instalacja-aktualizacja-odinstalowanie).

Wybór jest zapamiętany. Filmy na początku są pomijane: gra otwiera się na ekranie gracza. Utwórz
gracza (albo wybierz swojego) - pojawi się mapa strategiczna z 20 misjami kampanii.

## 3. Co nowego w grze

Misje grają się tak jak zawsze. Co się zmieniło:

- **Okno albo pełny ekran, dowolna rozdzielczość.** Obszar gry w oknie ma dokładnie wybraną
  rozdzielczość, obraz i mysz zgadzają się w oknie, a okno startuje na środku ekranu (i nie wraca
  już poza ekran, gdy grę zamknięto zminimalizowaną).
- **Ekrany panoramiczne.** Na dużym ekranie widać więcej mapy, a interfejs jest przy krawędziach.
  Misja 1 (mapa 1024 x 768) i menu są powiększane do wysokości okna. Obiekty i cienie są rysowane na
  całym ekranie.

  ![Misja w 1920 x 1080](img/widescreen.jpg)

- **Pasek stanu na całą szerokość.** Na szerokim ekranie pasek na dole biegnie od krawędzi do
  krawędzi zamiast kończyć się na środku.
- **Drugi rząd w panelu fabryki.** Fabryka może budować do 14 rodzajów wagonów (klawisze F1 - F12,
  `[` i `]`), ale jej panel pokazywał dziesięć. Jedenasty i dalsze są teraz w drugim rzędzie nad
  pierwszym i można je klikać.
- **Środkowy przycisk myszy** przeciąga mapę, także w misjach.
- **Stała liczba klatek** (domyślnie 60) zamiast zajmowania przez grę całego rdzenia procesora.
- **Postępy przechodzą z oryginału.** Profil z oryginału (wygrane misje, wyniki, ustawienia) jest
  przejmowany przy pierwszym uruchomieniu OpenLocoland.
- **Poprawki** awarii i błędów oryginału: awaria, gdy punkt naprawczy przejmował wagon wroga,
  awarie niektórych skryptów misji, rodzaj wagonu, którego fabryka nigdy nie mogła zbudować, mysz
  utykająca na dole ekranu na drugim monitorze po lewej lub nad głównym, brakujące dźwięki, działo
  fortyfikacji wysuwające się z dołu w każdym kierunku i inne.
- **Tłumaczenia** są uzupełnione (polskie i angielskie): brakujące podpowiedzi i nazwy jednostek.
  Mina (ładunek wybuchowy, **Mina**) i kopalnia (budynek, **Kopalnia**) mają wreszcie osobne nazwy.

## 4. Własne mapy i POLIGON

Mapa strategiczna ma **drugą stronę**: przycisk `►►►` w jej prawym dolnym rogu prowadzi do
**WLASNE MAPY**, `◄◄◄` z powrotem do kampanii.

![Strona własnych map](img/custom-maps-pl.jpg)

- **EDYTOR MAP** (pierwszy wiersz) uruchamia edytor - zob. [edytor map](#5-edytor-map).
- Pod nim są własne mapy: te dołączone do gry i te, które zrobisz albo dostaniesz od znajomych -
  każdy folder mapy w `maps\` folderu gry, znaleziony sam (folder skopiowany albo usunięty pojawia się
  lub znika nawet przy otwartej stronie). Długa lista się przewija - strzałki obok niej, kółko myszy,
  klawisze w górę i w dół.
- Mapa, której nie da się otworzyć (jej folder zniknął, brak obrazu terenu albo skryptu), mówi dlaczego
  w wierszu pod listą.
- Kliknięcie mapy pokazuje jej odprawę; **URUCHOM GRE** gra ją jak misję kampanii.
- Młotek i klucz na prawo od tytułu mapy (**Edytuj mape**) otwierają tę mapę w edytorze.

![POLIGON](img/poligon-pl.jpg)

**POLIGON** to gotowa misja zrobiona tymi samymi narzędziami co w edytorze: wróg zbudował bastion
na wschodzie - bramy strzegą dwa działa fortyfikacji, za nią stoi jego fabryka i punkt naprawczy, a
z fabryki co jakiś czas wyjeżdżają nowe pociągi. Przebij się i zniszcz je. Pociągi wroga patrolują
jego linie, a pętla toru na wschód od dolnej linii daje miejsce na manewry.

Własna mapa nie jest misją kampanii: jej wygranie nie odblokowuje misji, nie dodaje do wyniku
kampanii, stopnia ani galerii chwały. Ma własną tabelę wyników, „ponów misję” w menu Esc gra ją od
nowa, a po niej menu wraca na stronę własnych map.

## 5. Edytor map

Edytor to własny tryb edycji silnika, w którym autorzy robili swoje mapy i którego gra nigdy nie
udostępniła: co postawisz, to gra zagra. Otworzysz w nim każdą mapę - nową, POLIGON albo misję
kampanii (mapę kampanii zapisz pod własną nazwą, zanim zaczniesz ją zmieniać; POLIGON i szablon
też - każda aktualizacja gry zapisuje je od nowa, więc edytor zawsze zapisuje je pod nową nazwą,
proponując np. `poligon1`).

**Uruchamianie:** **EDYTOR MAP** na stronie własnych map - gra uruchamia się od nowa w edytorze, na
nowej pustej mapie: 2048 x 1536, trawa, jeszcze bez nazwy (NOWA MAPA). Skrót na pulpicie **Locoland -
edytor map** od razu otwiera taką mapę. `Ctrl+S` (albo Menu → **ZAPISZ MAPE**) ją zapisuje: za
pierwszym razem pyta o nazwę - wpisz np. `most_1`, a mapa trafi do własnego folderu
`maps\most_1\most_1.map` i na stronę własnych map. Nigdy niezapisana nowa mapa znika, gdy ją opuścisz
(inna mapa, powrót do menu, wyjście), więc wypróbowanie edytora niczego nie zostawia.

![Edytor](img/editor-factory-pl.jpg)

Co widać:

- **Mapę** tak, jak ją zrobiono, w bezruchu: poziom jest *wstrzymany* - nic się nie rusza, żaden
  skrypt nie działa. Poza mapą jest przyciemniona siatka, a krawędź mapy ma ramkę (można przewinąć
  300 px poza krawędź: tory biegną tam dalej).
- **Pasek** na dole w wyglądzie paska stanu gry: narzędzia 1 - 0 w jego gniazdach, kolejne na
  **listwie** nad nim. Wybrane jest w ramce. Nad listwą wybrane narzędzie ma swój **rząd** ustawień:
  rodzaj szyny i rampę dla Torów, tekstury albo podsypkę dla Malowania terenu, listę wagonów dla
  Pojazdów, obiekty tematu dla Wszystkich obiektów.

  ![Rząd narzędzia Tory nad paskiem](img/track-row-pl.jpg)

- **Wiersz stanu** u góry: narzędzie (albo to pod myszą), liczba kroków do cofnięcia i ponowienia,
  „nie zapisano” i ostatni komunikat - dlaczego czegoś nie da się zrobić, zawsze jest napisane tutaj.
- **Podpowiedzi**: gdy kursor chwilę stoi, kilka słów obok niego podaje klawisze narzędzia (albo
  pozycji paska pod nim), np. dla Torów „klik: początek / koniec, kółko: rampa, V: rodzaj”; następny
  ruch je zabiera. **PODPOWIEDZI POD KURSOREM** na górze panelu Menu je wyłącza i włącza.
- **Muzyka** w prawym górnym rogu: nazwa pliku, `|<` poprzedni, `>` gra go podczas edycji (`||`
  zatrzymuje), `>|` następny - żeby przesłuchać muzykę gry i wybrać muzykę mapy w Ustawieniach misji. Na
  początku wyłączona; edytor pamięta, czy grała i który plik. **TEST BEZ ZAPISU** i **ZAGRAJ
  ZAPISANA MAPE** grają własną muzykę mapy.
- **Przyciski ? i menu** na lewym końcu paska (tam, gdzie gra ma przycisk pauzy). **?** (albo F1)
  otwiera pomoc: 29 stron z tym, co jest w tym rozdziale, w skrócie. Zaczyna się od **spisu
  treści**: kategorie (PODSTAWY, TORY I POCIAGI, OBIEKTY, MISJA I ZAPIS, TEREN, PRZYJAZDY I FABRYKI)
  po lewej, strony tej pod myszą po prawej - kliknięcie otwiera stronę. Strona ma swoją kategorię za
  tytuł i swój numer (5 / 29); strzałki, Page Up / Down i kółko zmieniają strony, a **SPIS TRESCI**,
  `Esc` albo prawy przycisk wracają do spisu.

  ![Spis treści pomocy](img/help-contents-pl.jpg)

  ![Strona pomocy](img/editor-help-pl.jpg)

  **Przycisk menu** otwiera menu edytora ze wszystkimi poleceniami - **NOWA MAPA**, **WCZYTAJ MAPE**,
  **ZAPISZ MAPE**, **EKSPORT MAPY**, **IMPORT MAPY**, **ZMIEN NAZWE / USUN**, **USTAWIENIA MISJI**, **ROZMIAR MAPY**, **ZAGRAJ
  ZAPISANA MAPE**, **COFNIJ**, **PONOW**, **TEST BEZ ZAPISU**, **POWROT DO MENU**, **WYJDZ Z GRY**;
  wiersz stanu mówi, co robi to pod myszą. Otwiera je też `Esc`, gdy żaden panel nie jest otwarty i
  nic nie jest w toku.

  ![Menu edytora](img/editor-menu-pl.jpg)

**Poruszanie się:** krawędź ekranu, klawisze strzałek albo przeciąganie środkowym przyciskiem myszy.

**Podstawy edycji** (narzędzie Zaznacz, klawisz 1):

| Czynność | Jak |
|---|---|
| zaznaczenie | kliknij obiekt (wagon lub jednostka przed każdym innym obiektem, one przed szynami; najmniejszy z nich pod kursorem); oznacza go kółko |
| przesunięcie | przeciągnij go |
| obrót | R / E albo kółko myszy |
| wyżej / niżej | PgUp / PgDn o 4 (z Shift o 16) albo Shift+kółko; Home stawia z powrotem na ziemi - wiersz stanu pokazuje wysokość nad ziemią |
| zmiana armii | A: nasi, wróg, neutralni, piraci - flagi zmieniają kolor (przy stawianiu A ustala armię następnego obiektu) |
| usunięcie | Del (Shift+Del: cały pociąg); narzędzie Usun: kliknięcie, albo przeciągnięcie z wciśniętym przyciskiem usuwa wszystko, nad czym przejdzie kursor (jeden krok cofania); Shift+klik: cały odcinek toru |
| cofnij / ponów | Ctrl+Z / Ctrl+Y - każdą zmianę; pamięta ostatnie 500 kroków |
| panel obiektu | prawy przycisk myszy na nim |

**Stawianie.** Obiekt staje tam, gdzie go postawisz, także na innych obiektach. Przy szynie fabryka,
punkt naprawczy, fortyfikacja, kopalnia, semafor, skrzynka z pieniędzmi, wrak i pirackie urządzenia
toru przyciągają się do toru tak, jak stoją w misjach - podgląd pokazuje gdzie, a wiersz stanu mówi
"na tor"; z wciśniętym **Shiftem** albo z dala od toru obiekt zostaje tam, gdzie kursor, obrócony tak,
jak wybrałeś (fabryka ani punkt naprawczy nie dostają tam linii, semafor też nie). **Mina** staje tylko na
punkcie toru. Nic nie staje we **wrotach fabryki** (w jej środku): fabryka usuwa to, co tam stoi, gdy
buduje wagon. **Flaga** powiewa na własnym maszcie tam, gdzie ją postawisz, z wiatrem jak wszystkie flagi
w misjach; z Shiftem nad masztem albo dworcem trafia na niego w miejsce jego flagi. R / E obracają
flagę masztu, Shift+R / E flagę dworca.

**Panel obiektu** (prawy przycisk) ma jego armię, obrót, wysokość (niżej, wyżej, na ziemi) i usuwanie, kolor semafora i linię przy
nim, a dla fortyfikacji, kopalni i cytadeli - działo w każdym gnieździe (puste, działo albo
rakiety; gdy panel jest otwarty, gniazda są ponumerowane na murze, a kliknięcie działa otwiera panel
jego budowli). Prawy przycisk gdzie indziej albo Esc go zamyka. W każdym panelu - obiektu,
przyjazdów, fabryki - wiersz stanu u góry mówi, co robi wiersz panelu pod myszą.

![Panel fortyfikacji](img/editor-fort-panel-pl.jpg)

## 6. Samouczek: twoja pierwsza misja

Zrobimy małą misję: pociąg gracza przyjeżdża zza zachodniej krawędzi, gracz ma fabrykę i punkt
naprawczy, wróg ma na linii fortyfikację z dwoma działami i patrolujący pociąg. Misja jest wygrana,
gdy wrogowi nie zostanie żadna jednostka.

### 1. Nowa mapa

Edytor startuje na pustej mapie 2048 x 1536 z trawą - możesz zrobić misję od razu na niej i zapisać
ją pod własną nazwą (`Ctrl+S` za pierwszym razem o nią pyta, np. `most_1`). Dla innego terenu albo
rozmiaru: przycisk menu → **NOWA MAPA**:

![Okno Nowa mapa](img/new-map-pl.jpg)

- **Nazwa** - nazwa mapy, która jest też jej folderem w `maps\` (np. `most_1`: mapa to
  `maps\most_1\most_1.map`). Do 64 liter angielskich, cyfr, spacji i znaków dozwolonych w nazwach
  plików Windows.
- **Teren** - *pusta mapa* z jednej tekstury (trawa, piasek, śnieg, skała ... - płaska i goła, do
  malowania), *łąka* albo teren jednej z map kampanii.
- **Rozmiar** - dla pustej mapy albo łąki: od 512 do 4096 px w każdą stronę co 256 (`-` i `+`) albo
  gotowy: 1024 x 768 (mała, jak misja 1), 2048 x 1536 (większość misji), 3072 x 2304 (duża). Na
  początek weź 2048 x 1536.

OK - edytor otwiera nową mapę. Ma już skrypt misji i teksty zrobione z szablonu; uzupełnisz je w
kroku 10. (**ROZMIAR MAPY** w menu zmienia rozmiar później: mniejsza mapa traci teren i obiekty za
nową krawędzią - tor może biec jeszcze 300 px dalej, poza zasięgiem wzroku.)

### 2. Pomaluj teren

**Malowanie terenu** z listwy: jego rząd przycisków nad paskiem ma tekstury (trawa, jasna trawa,
ściółka, piasek, pustynia, śnieg, lód, skała, ciemna skała, lawa, bagno, dno jeziora - wzięte z map
kampanii; `[` i `]` też je wybierają), **ROZMIAR** (8 - 200 px; także kółko myszy), **SILA** (ile
pokrywa jedno pociągnięcie: mniejsza pozwala mieszać tekstury) i **KRAWEDZ** miękką albo twardą. Maluj
przeciągając lewym przyciskiem; prawy (albo Shift+przeciąganie) przywraca oryginalny obraz; Alt+klik
pobiera teksturę spod kursora (także z obrazu pomalowanego i zapisanego wcześniej). Namaluj
piaszczystą drogę tam, gdzie pobiegną tory, i kilka plam innych tekstur. Pomalowany obraz zapisuje
się razem z mapą.

### 3. Woda, bagno, lawa

**Woda i lawa** z listwy: `W` wybiera wodę, bagno albo lawę; przeciągaj, żeby ją położyć. Obniża teren
w nieckę z brzegiem, kładzie powierzchnię wody i dodaje jej efekty (kręgi na wodzie, bąble, wybuchy
lawy) oraz region, w którym wybuchy rozpryskują wodę. Shift+przeciąganie ją usuwa. Jeden rodzaj
wody ma na mapie jeden poziom, tak jak w misjach.

Drugi sposób to **Region jasnej wody** (Wszystkie obiekty, temat **Efekty**): prostokąt wody o prostych
brzegach, bez brzegu terenu i bez efektów - sadzawka, zalana łąka albo dalsza część tafli jeziora.
Postawiony leży 2 poziomy nad najwyższym terenem pod nim, więc jego wodę widać nad terenem; postawiony
nad jeziorem jasnej wody z narzędzia wody bierze poziom tego jeziora (jedna tafla). Jego rozmiar
zmienia się o całe kafle wody (128 px). Co wybrać: jezioro, rzeka albo lawa z brzegiem - narzędzie
Woda i lawa; płaska tafla jasnej wody - region.

Trzeci sposób jest taki jak w misjach (strona pomocy 32): rząd **TWORZY** narzędzia **Region** - jasna
woda, ciemna woda, bagno, bagno 2 albo lawa. Przeciągnięcie kładzie taflę tej wody na prostokącie na
jednym poziomie (nową tuż nad terenem), Shift+przeciągnięcie ją zabiera, **CALA MAPA** kładzie ją wszędzie
pod terenem, a **POZIOM** `-` `+` podnoszą albo obniżają całą taflę. Widać ją tylko tam, gdzie teren jest
niżej niż jej poziom: obniż teren pod nią Wysokością terenu, żeby zrobić jezioro albo jamę z lawą
(trzymaj ją 5 poziomów albo więcej pod terenem tam, gdzie ma jej nie być widać), albo podnieś taflę,
żeby zalać teren. Skrypt mapy może ją podnosić i obniżać w czasie gry, jak powódź w misji 15; edytor
sam tego nie dopisuje.

Narzędzie **Wysokosc terenu** kształtuje teren swoim rzędem przycisków nad paskiem: tryb -
**PODNIES/OBNIZ** (lewy przycisk podnosi, prawy obniża), **WYROWNAJ**, **WYGLADZ**, **RAMPA** (przeciągnij
z jednego punktu do drugiego: równy spadek między ich poziomami, np. podjazd na płaskowyż) i
**NIEROWNOSCI** (lekkie losowe nierówności); **ROZMIAR** (także kółko myszy), **SILA** (jak szybko działa) i
**KRAWEDZ** miękka albo twarda (twarda: urwiska i płaskowyże). Shift wyrównuje, a Ctrl wygładza na
jeden ruch bez zmiany trybu; `M` przełącza tryby. Pędzel działa dalej, gdy trzymasz go w miejscu, a
poziom pod kursorem zawsze widać w wierszu stanu. Obraz terenu idzie za
wysokościami: wzgórze widać jako wzgórze z cieniem i zasłania to, co stoi za nim, w edytorze i w grze
(obraz zapisuje się z mapą; na terenie misji kampanii staje się własnym obrazem mapy). Poziomice co
4 poziomy pokazują wysokości, gdy narzędzie jest wybrane; `H` pokazuje je przy każdym narzędziu.
**Wyrównywanie** bierze poziom miejsca, w którym zaczynasz - zacznij na torze, żeby zrobić płaskowyż
na wysokości toru - albo poziom pobrany przez Alt+klik. Obiekty zostają na ziemi; torów i terenu tuż przy nich narzędzie nie rusza - **TORY: NIE ZASLANIANE**
w jego wierszu -, więc szyny zostają widoczne w wykopie; **ZASLANIANE** pozwala pędzlowi zmieniać i je,
a teren może przykryć szyny.

Obiekt może też stać wyżej albo niżej sam: zaznacz go i naciśnij `PgUp` / `PgDn` (albo Shift+kółko,
albo WYSOKOSC w jego panelu); `Home` stawia go z powrotem na ziemi. Wagony i szyny zostają na torach.
Kilka jednostek i dział gra zawsze sama stawia na ziemi; wiersz stanu to mówi.

### 4. Główna linia

**Tory** (klawisz 3). Kliknij, żeby zacząć odcinek - na końcu szyny mapy albo gdziekolwiek - a
następne kliknięcie kładzie go w stronę kursora - **jedną szynę**, gdy kursor jest blisko początku,
a cały **ciąg** prostych i łuków w jego stronę, gdy jest dalej - i kończy go; kolejne kliknięcie
zaczyna nowy odcinek (na końcu szyny w promieniu 20 px, więc dalszy ciąg to jedno kliknięcie więcej).
Prawy przycisk (albo Esc) anuluje zaczęty odcinek, a kliknięcie kółkiem myszy usuwa szynę pod kursorem (Ctrl+Z ją
przywraca). Podgląd zawsze pokazuje szyny, które kliknięcie położy; narzędzie kładzie to, co
klikniesz - na innym torze, przez jego styki, ślepy koniec, ostry skręt, tę samą szynę jeszcze raz.
Nie zostanie położona tylko 7. szyna w jednym punkcie (gra pozwala na 6): jej końce są **czerwone**,
a wiersz stanu to mówi. Dobrze wiedzieć:

- krok to 71 x 50 px albo jego połowa (siatka torów);
- skręt zawsze przez **łuk** (prosta szyna nie łączy się wprost ze skośną); ostrzejszy skręt zostanie
  położony, ale pociągi zawracają w tym miejscu;
- **rozjazd**: zacznij nową linię w punkcie istniejącej - odgałęzienie to prosta szyna i łuk o tym
  samym środku, **rozjazd Y** jak w misjach;
- **skrzyżowanie**: połóż linię w poprzek dowolnego toru, także przez punkt, w którym stykają się jego
  szyny; pociągi je przejeżdżają, ale nie skręcą tam na drugi tor. Strona 23 pomocy opisuje siatkę.

Główną linię połóż tak, jak robią to misje: **zza zachodniej krawędzi** (przewiń nad siatkę) przez
mapę, żeby pociągi przyjeżdżały i odjeżdżały poza widokiem. Potem bocznica na fabrykę: odgałęzienie
od linii przez Y, prosty odcinek kończący się **ślepym końcem**.

Rząd nad paskiem wybiera rodzaj następnych szyn - zwykłe, bagienne (na nasypie), zniszczone albo
zniszczone bagienne, każdy pokazany swoją szyną - i czy szyny idą w górę, płasko albo w dół; `V`
też przełącza rodzaje. `PgUp` / `PgDn` albo kółko myszy sprawiają, że każda kładziona szyna wznosi się albo opada o stopień po poprzedniej, dopóki tego nie zmienisz (rampy na
wyższy teren: każda rampa to 11 poziomów; wiersz stanu pokazuje poziom). Szyny na ziemi mają teren
tuż nad sobą (wzgórze na ich drodze zostaje ścięte do nich); rampa albo podniesiona linia zostawia teren
pod sobą taki, jaki był, i stoi na podsypce, która wygląda jak nasyp - chyba że w rzędzie **TEREN POD
WZNOSZACYMI SZYNAMI** (albo `G`) jest **LAGODNIE** lub **OSTRO**: wtedy teren pod wznoszącymi się szynami
jest podnoszony, z szerokim albo krótkim stokiem (jedno cofnięcie razem z szyną). Linię na wzgórze najpierw wprowadź po rampach, potem podnieś teren
wokół jej końca i wyrównaj go, zaczynając na torze. `B` wybiera podsypkę pod nowe szyny. Kliknięcie
kółkiem, Del albo narzędzie Usun usuwa szynę; Shift+klik narzędziem Usun usuwa cały odcinek.

**Podsypka.** Podsypka to obiekt gry pod każdą szyną, jak w większości misji. Nowe tory dostają
podsypkę wybraną w rzędzie narzędzia Tory, **PODSYPKA** (łąka, lód, wulkan, pustynia albo brak; `B`
przełącza). Żeby podmienić podsypkę pod gotowym torem - zwykłą na wulkaniczną - wybierz jedną z
czterech podsypek tematu **Dekoracje** i kliknij albo przeciągnij po torze: szyny w kole pędzla
dostają tę podsypkę zamiast dotychczasowej, a Shift zabiera ich podsypkę (także luźną, zostawioną
przez starszą wersję edytora). Nic nie zostaje luzem na ziemi, obraz terenu nie jest ruszany.

**Malowanie terenu** → **TRYB: PODSYPKA** to co innego: wmalowuje pas żwiru w obraz terenu pod
szynami, po których przejdzie pędzel, tak jak zrobiły to trzy misje zamiast kłaść obiekty - kropki
wzdłuż szyn pokazują, gdzie legnie: **RODZAJ**, **SZEROKOSC** (28 px, jak w misjach), **ROZMIAR**,
**SILA**, **KRAWEDZ**; pas idzie wzdłuż łuków, rozjazdów i ramp. Obiekty podsypki tych szyn zostają
(zabierz je Shiftem z podsypek Dekoracji, jeśli chcesz sam pas). Prawy przycisk przywraca oryginalny
obraz, Alt+klik pobiera rodzaj, **POD CALA MAPA** maluje pas pod każdą szyną mapy naraz. Jedno
pociągnięcie to jedno cofnięcie.

![Podsypka pod rozjazdem](img/paint-bed-pl.jpg)

### 5. Przyjazdy pociągów

**Przyjazdy pociagow** (listwa) otwiera panel z pociągami, które w czasie misji wjeżdżają na mapę -
pociąg gracza na start, posiłki, fale wroga. Najpierw jest ich lista, każdy przyjazd w dwóch wierszach
zwykłych słów („1. NASI, WJAZD 1: Lokomotywa + WAGONY: 1”, „NA POCZATKU, RAZ”); kliknięcie otwiera
przyjazd, **+ NOWY** tworzy nowy, **KOPIUJ n** i **USUN n** dotyczą ostatnio otwartego.

![Lista przyjazdów](img/arrivals-pl.jpg)

W przyjeździe każdy wiersz to jedna rzecz (nowy to pociąg naszych, który jedzie do punktu):

- **KTO** - nasi, wróg, neutralni albo piraci;
- **SKLAD** - w kolejności pociągu: **+ WAGON** dodaje na koniec z palety Pojazdy, **SZABLON...**
  pokazuje listę składów autorów z nazwami; kliknięcie wagonu go wybiera - **<** i **>** go przesuwają,
  **USUN** go zabiera. Przytrzymaj wagon (albo **+ WAGON**), a nad nim otworzą się jednostki: puść
  na jednej, by wstawić ją w miejsce tego wagonu (albo dodać). Lokomotywa może stać w dowolnym miejscu
  składu;
- **WJAZD** - **WSKAZ**, potem kliknij główną linię **za zachodnią krawędzią**: tam powstaje
  pociąg;
- **KIEDY** - **NA POCZATKU**, **PO** czasie, **GDY GRACZ NIE MA POCIAGU** albo **GDY GRACZ ZAPLACI**
  kwotę (te dwa **NIE WCZESNIEJ NIZ** po czasie);
- **POWTORZ** - **RAZ**, ileś **RAZY** albo **BEZ KONCA**, **CO** tyle sekund;
- **POTEM** - pociąg naszych, neutralnych i piratów **STOI**, **JEDZIE DO PUNKTU** albo **PATROLUJE**
  między dwoma punktami; wroga **ATAKUJE**, **PILNUJE PUNKTU** albo **PATROLUJE** (robi to AI gry).
  Punkty **WSKAZ** na torze, poza główną linią;
- **KOMUNIKAT** u góry po prawej, **TAK** albo **NIE** („przybyły posiłki”, dla naszych i wroga).

Pasek nad tytułem podaje prawdziwy problem z przyciskiem, który go naprawia - brak wjazdu (**WSKAZ
WJAZD**), brak świateł na wjeździe (**POSTAW SWIATLA**: para semaforów w pierwszym miejscu w mapie i ich
linia, która wpycha pociąg - linia wjazdu i jej światła to jedno, jak w misjach) albo światła na drodze
bez linii do nich (**LINIA DO SWIATEL**), brak lokomotywy w składzie, brak punktu. Fala czeka, gdy na wjeździe stoi pociąg. Strzałka na dole po lewej, Esc albo prawy przycisk
wracają do listy; kliknięcie wjazdu albo punktu na mapie otwiera jego przyjazd. Każdą zmianę cofa
Ctrl+Z. Mapy ze starszej wersji zachowują swoje posiłki i pociągi startowe wroga ze skryptu (widać je
jako znaczniki na torze; kliknięcie znacznika otwiera ten panel), dopóki w panelu nie wybierzesz
**PRZESTAW NA PRZYJAZDY**: pociągi startowe stają wtedy na mapie jako zwykłe pociągi z rozkazami
(patrol albo atak), które edytuje się jak każdy pociąg.

### 6. Fabryka, punkt naprawczy, linie jednokierunkowe i semafory

**Fabryka** (klawisz 5) i **Punkt naprawczy** (klawisz 6) stają tam, gdzie stawiali je autorzy -
podgląd pokazuje gdzie; z dala od toru albo z Shiftem zostają tam, gdzie kursor, bez linii:

- **fabryka** na **ślepym końcu bocznicy**, obrócona wyjazdem na zewnątrz. Dostaje swoją **linię
  wyjazdu**: linię jednokierunkową wzdłuż bocznicy (zbudowane wagony pojawiają się w fabryce i są
  wypychane) i czerwony semafor przy niej;
- **punkt naprawczy** na **prostym odcinku** toru, obrócony wzdłuż niego jak brama. Dostaje linię
  jednokierunkową przez siebie (pociągi są przez niego przepychane, a każdy wagon naprawiany) i
  swoje semafory. `R` albo `E` (albo **OBROT: ZAWROC** w jego panelu) obraca go na torze:
  linia biegnie wtedy w drugą stronę, a semafory zamieniają się miejscami.

Przeciągnięcie przenosi je razem z linią na najbliższe właściwe miejsce (upuszczone z dala od toru
albo z Shiftem: tam, gdzie upuszczone); usunięcie usuwa też linię.
**Bloker** (kozioł oporowy, w grupie KOLEJ we Wszystkich obiektach) stawia się swobodnie i obraca
R / E jak każdy obiekt: ustaw go sam na końcu toru.

**Linie jednokierunkowe** (**Linia jednokierunkowa**, klawisz 4): linia na torze, na którą żaden
pociąg nie wjedzie pod prąd - tak misje oznaczają wjazd na mapę, wyjazd, fabryki i punkty naprawcze.
Kliknij jej początek i koniec na torze - na mapie linia ma kreskę na początku i strzałki w stronę,
w którą biegnie, a mysz nad nią mówi, co robi; potem na zaznaczonej linii - wiersz stanu podaje jej początek, koniec i to, co robi; także narzędziem
Zaznacz kliknięcie kreski lub strzałki linii ją zaznacza, a prawy przycisk otwiera jej panel - `T` włącza albo wyłącza
pchanie, `R` ją odwraca, `S` stawia jej semafory, Del ją usuwa. Zrób dwie:

- **wjazd na mapę**: od końca głównej linii za krawędzią na mapę, z pchaniem - jeśli zrobiło go
  **POSTAW SWIATLA** przyjazdu (krok 5), już jest;
- **wyjazd z mapy** na drugim końcu, krótki i skierowany w głąb mapy, bez pchania - żeby żaden
  pociąg nie wyjechał poza mapę.

**Semafory** (klawisze 8 zielony, 9 czerwony) są tylko znakami - pokazują kierunek linii
jednokierunkowych i stoją obok toru, po prawej stronie pociągów, na które patrzą, jak w misjach:
staje po tej stronie toru, którą wskażesz (`R` / `E` albo kółko: po drugiej), a na postawionym `R` / `E`
obracają go - przechodzi na drugą stronę toru, do pociągów jadących w drugą stronę; wiersz stanu i jego
panel (DLA POCIAGOW) mówią, dla których jest. `T` zmienia kolor. Postawiony semafor od razu dostaje własną linię
jednokierunkową, tak jak mają je semafory misji, a podgląd ją pokazuje: wjazd przez **zielony** semafor jest rysowany na zielono, zatrzymanie przy **czerwonym** - krótkim czerwonym znakiem przed nim, kończącym się przy nim:
**zielony** wpuszcza pociągi, na które patrzy (linia biegnie od nich i przepycha je obok niego),
**czerwony** zatrzymuje je przy sobie (linia biegnie zza niego, a pod prąd nie wolno). Zielony i
czerwony naprzeciw siebie po dwóch stronach toru dzielą jedną linię - to wjazd na mapę, jeśli postawisz
je tam, gdzie wchodzi główna linia. Obrót, `T` albo przesunięcie zmieniają linię, usunięcie semafora
ją usuwa (chyba że ma ją jeszcze drugi z pary). Semafor postawiony przy końcu innej linii należy do niej i własnej nie dostaje;
`L` (zielony) i `Shift+L` (czerwony) i tak dają zaznaczonemu semaforowi własną linię, a wiersz LINIA
jego panelu ją zabiera (BRAK).

**Co buduje fabryka**: prawy przycisk na fabryce, **CO BUDUJE...**. Dla twojej fabryki (i neutralnej,
której lista będzie twoja po jej zdobyciu) kliknij jednostki, które może budować - zachowują kolejność i
klawisze gry F1-F12, [ i ]; dla ostatnio klikniętej **NA LISCIE** ustawia, od kiedy (od początku albo po
czasie), a **NAJWYZEJ** - ile ich gracz może mieć.

**Co fabryka buduje ze skryptu - jej plan**: fabryka wroga pokazuje pociągi, które buduje jego AI (AI ma
tylko wróg), a druga zakładka **BUDOWANE POCIAGI** twojej, neutralnej albo pirackiej - pociągi, które
skrypt mapy każe jej zbudować. Ich lista i ekran każdego, jak w panelu przyjazdów: **POCIAG** (jego
wagony po kolei - lokomotywa gdziekolwiek -, **+ WAGON**, **SZABLON...** pociągi autorów), **KIEDY**
(**O CZASIE**, **PO INNYM** - tyle po zrobieniu pociągu przed nim na liście - albo **GDY** zachodzi
warunek, np. gracz ma mniej pociągów niż n, nie przed czasem), **POWTORZ** (raz, n razy albo bez końca,
co tyle - **PO ZROBIENIU OSTATNIEGO** albo **PO STRACIE**) i **POTEM** (wroga atakuje, pilnuje punktu albo patroluje; pozostałych stoi, jedzie
do punktu albo patroluje). Pociąg twój, neutralnych albo piratów powstaje wagon po wagonie, gdy fabryka
jest wolna, a jego strona ma pieniądze na następny wagon (cena pod każdym), niezależnie od listy fabryki;
wróg buduje po jednym, gdy atakuje mniej jego pociągów, niż pozwala **AI...**. **AI...** ma pozostałe
ustawienia wroga od autorów, każde z linijką opisu: czy stracony pociąg startowy jest budowany na nowo i
po jakim czasie, wypady pociągów startowych, od kiedy są wypady i ile trwają, mądry atak oraz to, czy
jego działa strzelają do pociągów neutralnych (**POCIAGI NEUTR.: STRZELA DO NICH** albo **GDY OBERWIE**). Wszystko poza
pociągiem raz o czasie wymaga mapy z przyjazdami (starsze mapy: PRZESTAW w panelu przyjazdów).

![Plan fabryki wroga](img/factory-plan-pl.jpg)

### 7. Wróg

- **Fortyfikacja** (klawisz 7, wroga): staje **na torze**, obrócona wzdłuż niego - to brama, przez
  którą przejeżdżają pociągi (`R` albo `E` obraca ją tam). Jest od razu uzbrojona: jej panel (prawy przycisk) ustawia działo w każdym
  gnieździe - działo albo rakiety. Samej fortyfikacji nie da się zniszczyć; jej działa tak.
- **Wszystkie obiekty** (klawisz 0) otwiera paletę ze wszystkimi obiektami z misji, według tematów
  (łąka, las, pustynia, lód, wulkan, bagno, park, piraci, miasto, przemysł, kolej, flagi i skrzynie,
  wraki, jednostki i stworzenia, efekty): kliknij jeden, potem postaw go na mapie. Wiersz **ARMIA**
  pod ikonami ustala, czyj będzie - JAK W MISJACH, NASI, WROG, NEUTRALNI albo PIRACI (paleta Pojazdy
  zaczyna od naszych); `A` przy stawianiu robi to samo. Są tu **kopalnia** (jedno gniazdo na
  działo; staje na torze jak fortyfikacja, tak jak 29 z 30 kopalń w misjach), cytadela, mury, budynki, roboty. **Wrak** (spalony wóz
  albo wagon) postawiony albo przeciągnięty przy szynie staje na torze, wzdłuż niego (`R` go
  odwraca), tak jak w misjach; gdzie indziej zostaje tam, gdzie go postawisz. W grze pociąg najeżdża
  na niego i go miażdży - wrak pociągu nie zatrzymuje.

  ![Wszystkie obiekty](img/editor-palette-pl.jpg)

- **Cytadela** (piraci): staje nad **ukośnym torem**, razem z wieżą, kratami bramy i świdrami, jako
  jeden obiekt. W grze,
  dopóki stoi jej wieża, brama zatrzymuje i rani wszystko z innej armii niż wieża, co podjedzie
  blisko (poza neutralnymi); bez wieży jest niegroźna.
- **Charybda** (bagno): staje **na torze** - tor biegnie przez jej paszczę -, z okiem, zębami i
  działami, jako jeden obiekt. Wciąga pociągi innych armii, które ją mijają, a twoje zjada; jej działa
  śpią, dopóki jej nie zranisz. Zabij jej
  oko, żeby ją zniszczyć.
- **Gniazdo much** (jednostki i stworzenia): jego panel wybiera **ZIELONE, FALAMI** (fale zielonych
  much) albo **SZARE, NA STRAZY** (szare muchy, gdy twoje pociągi podjadą blisko, rój, gdy je
  zniszczysz).
- **Mina** (jednostki i stworzenia): staje na punkcie toru, w armii z wiersza palety. Edytor oznacza
  każdą minę jej armią (MINA: WROG); w grze widzi ją tylko jej armia - pociąg innej armii ją odpala,
  jej własne pociągi zatrzymują się przed nią (gdy poziom ruszy, mina zamyka im swój punkt toru).

- **Pociągi**: **Pojazdy** z listwy otwierają wagony - lokomotywy, wagony bojowe, zaopatrzenie
  (wóz z amunicją, pojazd naprawczy, cysterny), wagony cywilne, przeszkody; rząd nad paskiem pokazuje
  je wszystkie, grupa po grupie (strzałki przewracają strony, `[` `]` przechodzą po kolei, napis podaje
  grupę wybranego wagonu i jego numer, np. WAGONY BOJOWE 5 / 23). Wagon staje na torze
  pod kursorem (gdy blisko są dwa tory - na bliższym); postawiony przy końcu pociągu na tym torze
  dołącza się do niego. Zrób pociąg wroga: wybierz **WROG**
  w wierszu **ARMIA** palety (albo `A` przy stawianiu, albo `A` na postawionym pociągu), potem
  lokomotywa i działo. `U` odczepia przed zaznaczonym wagonem, `Shift+U` za nim (nic się nie przesuwa), `C` doczepia do pociągu spotkanego wzdłuż tego samego toru
  (nie do pociągu na torze obok), `R` zawraca pociąg, `E` obraca jeden wagon w jego miejscu
  (lokomotywa tyłem w środku pociągu, działo na końcu), przeciąganie kładzie pociąg na torze pod
  kursorem - gdy się tam nie mieści (za krótki ślepy tor, inny pociąg), zostaje, a wiersz stanu mówi
  dlaczego. Pociąg bez **lokomotywy** nie pojedzie.

  ![Rząd Pojazdów nad paskiem](img/vehicles-strip-pl.jpg)

- **Patrol**: panel pociągu (prawy przycisk) → ROZKAZY: **PATROL...**, potem kliknij drugi koniec na
  torze. Pociąg jeździ tam i z powrotem między miejscem, gdzie stoi, a tym punktem. Wskaż go w mapie:
  za wjazd na mapę, na którego linię jednokierunkową żaden pociąg nie wjedzie pod prąd, pociąg nie
  pojedzie - wiersz stanu to mówi.
- Więcej pociągów wroga: kolejne pociągi na mapie z rozkazami, przyjazdy wroga (krok 5) i pociągi,
  które buduje jego fabryka - jej plan (krok 6). Do tych ostatnich wróg potrzebuje fabryki: postaw ją
  (klawisz 5) na własnym ślepym końcu i `A` na wroga.
- **Skrzynka z pieniedzmi** (listwa) leży na torze: jednostka gracza, która ją zbierze, dostaje 100.

**Panel pociągu** (prawy przycisk na jednym z jego wagonów) - poza armią, obrotem, odczepianiem i
usuwaniem mapa z przyjazdami daje mu te wiersze (mapa ze starszej wersji ma sam **PATROL**, dopóki w
panelu Przyjazdy nie wybierzesz **PRZESTAW NA PRZYJAZDY**):

- **ROZKAZY: JEDZ DO...** punktu albo **PATROL...** między miejscem, gdzie pociąg stoi, a punktem, i
  **WALKA: ATAK**, **PILNUJ...** punktu albo **OBSZAR...** - dla pociągu każdej armii; punkt to
  kliknięcie na torze. ATAK, PILNUJ i PATROL wroga to zadania jego AI (z wagonem z działem - jego AI z
  samej lokomotywy robi taran); u pozostałych ATAK poluje na najbliższy wrogi pociąg, jeden po drugim,
  a PILNUJ jedzie na punkt i go trzyma (bierze wrogi pociąg w promieniu 150 px).
- **OBSZAR...** jest inny: pociąg nie jedzie nigdzie - stoi tam, gdzie stoi, i broni strefy wokół
  klikniętego punktu (gdziekolwiek na mapie, także z dala od pociągu). Gdy wrogi pociąg wjedzie do
  strefy albo zbliży się do niej na 120 px, pociąg rusza i atakuje najbliższego; wiersz **OBSZAR**
  ustawia promień (- 300 PX +) i **WRACA** (na swoje miejsce, gdy cel zginie albo odjedzie 240 px za
  strefę) albo **SCIGA** (za celem, aż zginie, potem na swoje miejsce). Zaznaczony pociąg pokazuje swoją
  strefę jako okrąg i kropkowaną linię od siebie do jej środka. Wrogi - według zasad mapy: neutralni nie
  zaczynają walki.
- **CEL: WSKAZ...** i kliknięcie jednostki albo budynku innej armii (fortyfikacja: jedno z jej dział) -
  pociąg atakuje go po starcie poziomu zamiast innego rozkazu; pociąg wroga przechodzi potem do swojego
  AI, pozostałe stają. **START: OD RAZU** albo **PO n S** wstrzymuje rozkaz do tego czasu.
- **ODBUDOWA: NIE / RAZ / 3 RAZY / ZAWSZE** i **ODBUDOWA ZA** (sekundy po stracie), dla pociągu wroga,
  neutralnych albo piratów z rozkazem: gdy jego lokomotywa zostanie zniszczona albo przejęta, AI wroga
  (w pierwszej wolnej fabryce) albo najbliższa fabryka jego armii buduje ten sam pociąg i daje mu ten
  sam rozkaz. Nie dla twoich pociągów. Pociąg piratów potrzebuje lokomotywy 10: piracką lokomotywę 5
  skrypty zabierają na starcie (oznacza posiłki).
- **NA CZERWONYM: STAJE** (jak każdy pociąg) albo **JEDZIE** - wtedy każdy pociąg tej armii może jechać
  linią jednokierunkową pod prąd, tam gdzie twoje pociągi i pociągi wroga są zatrzymywane (wyjazd z
  poziomu wypuszcza go też poza mapę, na ukryty tor). Jedna zasada dla całej armii.
- **STRZELA: SAMO** albo **NA ROZKAZ** - ten wiersz ma każda uzbrojona jednostka (neutralna: **W
  ODPOWIEDZI** albo **NIGDY**).

![Panel pociągu](img/train-panel-pl.jpg)

![Pociąg pilnujący obszaru](img/area-watch-pl.jpg)

Pociąg stojący na linii, która pcha (wyjazd z fabryki, tor punktu naprawczego, linia semafora), na
starcie poziomu jedzie do końca tej linii i tam zaczyna rozkaz. Punkt, do którego pociąg nie dojedzie -
za linią jednokierunkową, na którą nie może wjechać - edytor podaje przy wydaniu rozkazu i w opisie
wiersza ROZKAZY w wierszu stanu: w grze taki pociąg dojeżdża do linii i staje.

**Armie.** Na mapie edytora neutralni (armia 2) nigdy nie strzelają pierwsi: gdy ktoś trafi neutralne
działo, fortyfikację albo pociąg, ono i neutralni, którzy to widzą, odpowiadają tylko tej stronie - do
końca misji, a ona może wtedy strzelać do nich. Piraci (armia 3) walczą ze wszystkimi stronami, a
wszyscy z nimi. Panel jednostki neutralnych albo piratów zmienia to dla całej jej armii: **W WALCE:
ODPOWIADA NAPASTNIKOM** (zwyczaj neutralnych) albo **WSZYSCY** (zwyczaj piratów). Misje kampanii mają
dawne zasady gry. Neutralna fabryka albo punkt naprawczy staje się twój, gdy podjedzie do niego twój
pociąg (fortyfikacja albo kopalnia - gdy nie ma już ich dział).

Pamiętaj o zasadzie gry: fabryka, punkt naprawczy i fortyfikacja stoją **na torze albo tuż przy
nim**.

### 8. Ozdoby

**Dekoracje**: trawa, krzaki, kwiaty, kamienie, bruk, podsypki (zmieniają podsypkę torów, zob.
„Podsypka” wyżej), wraki... - temat **Dekoracje** we
Wszystkich obiektach, każda dekoracja na jednej liście (`[` `]` z dowolnego narzędzia tam prowadzą).
Rząd nad paskiem pokazuje temat (jego strzałki zmieniają strony); nazwa tej pod wskaźnikiem pokazuje
się nad nią, kliknięcie tam wybiera jedną, tak samo `[` `]`. Kliknięcie
na mapie stawia jedną tak, jak ją widać (`R` `E` ją obracają), przeciąganie rozrzuca je w promieniu
(kółko, `+` `-`) w losowych wyglądach, nigdy bliżej niż odstęp (`<` `>`). Shift+przeciąganie je zbiera.
Dekoracja wybrana w dowolnym temacie **Wszystkich obiektów** stawia się tak samo. Drzewa, domy i reszta
też są we Wszystkich obiektach, a gdy jeden z nich ma być postawiony, rząd nad paskiem pokazuje tak samo
jego temat.

**Pomoce przy stawianiu** (rząd nad paskiem, gdy coś stawiasz; na początku wszystkie wyłączone; strona
pomocy 30): **SIATKA** (`Ctrl+G`) stawia na punktach siatki o wybranym rozmiarze - części mostów, latarnie
i płoty w rzędzie; **NA TOR** (`N`) kładzie obiekt, który znajdzie się blisko szyny, na jej linii, most
łukowy na jego wysokości pod szynami jak w misjach (obracasz go `R` `E`); **WYSOKOSC** (`PgUp` / `PgDn`,
`Home`) ustawia wysokość przed postawieniem, podgląd ją pokazuje; **OBROT: LOSOWY** (`Q`) obraca każdy
obiekt inaczej, podgląd pokazuje następny obrót (`R` `E` losują inny); **STAWIAJ: POJEDYNCZO / ROZRZUC**
(`X`), osobno dla każdego obiektu: trawa, krzaki i podobne rozrzucają się przy przeciąganiu, reszta idzie
pojedynczo (dekoracja wraku już się nie rozrzuca), a drzewo albo kwiat też można rozrzucać.

**Kopiuj i wstaw**: `Ctrl+C` kopiuje zaznaczony obiekt (fort z działami), `Ctrl+V` wstawia kopię pod
wskaźnikiem, `Ctrl+D` obok oryginału - każde to jedno cofnięcie. Szyn i wagonów się nie kopiuje, a
skopiowana fabryka albo semafor nie ma własnej linii, dopóki nie przeciągniesz jej na tor.

**Zestawy z misji** (Wszystkie obiekty): to, co autorzy zawsze stawiali razem, jest jednym obiektem, a
jego panel wybiera część - słup graniczny ze świdrem (cały albo złamany), wyjście z poziomu z jego
szczytem, maszt albo dworzec z flagą, kominy z dymem albo poświatą, szyb naftowy, basen. **Zgniatacz**
(piraci) stoi w poprzek toru i co 25 sekund gryzie wszystko poza swoją armią; jego panel mówi, kiedy
staje - gdy padną wieże jego armii, albo nigdy. **Scylla** (bagno) na ukośnym torze gryzie każdego co
15 sekund. Gdy padną wieże piratów, świdry się łamią. **Region** (listwa): przeciągnij jego prostokąt; jego panel ustawia mgłę (mgła, pył,
opar, poświata) i rozpryski, nad nim albo nad całą mapą; stworzenie postawione w środku trzyma się go. Klik w środku regionu, gdzie nic innego nie stoi, zaznacza go (narzędzie Usun i `Del` go usuwają, prawy przycisk otwiera jego panel).
Mgła (wiersz **NAD** osobno) jest najgęstsza nisko, obiekty z niej wystają: nad regionem przybiera
kształt zagłębienia (dna jeziora), na płaskim terenie to zabarwiony prostokąt; **NAD: CALA MAPA** robi z
niej mgiełkę nad całym widokiem, jak w misjach 7, 13 i 18 (zamienia też wybuchy regionu w pluski na całej
mapie). Temat **Efekty** we Wszystkich obiektach ma region
jasnej wody i region z kręgami w deszczu; wodę widać tylko tam, gdzie teren pod nią jest niżej niż
region, więc region jasnej wody stawia się nad terenem (zob. 3. Woda, bagno, lawa).

### 9. Zobacz, jak działa

Menu → **TEST BEZ ZAPISU** (albo `Ctrl+R`): mapa działa w edytorze taka, jaka jest teraz, zapisana czy
nie, tak jak w grze - pociągi jeżdżą, działa strzelają, przyjeżdżają pociągi z panelu Przyjazdy. Nic
nie pyta: twoja mapa zapisuje się najpierw, a POLIGON, szablon, mapa kampanii i nowa mapa bez nazwy
działają z ukrytej kopii, więc zostają, jakie były. `Ctrl+R` wraca do edytora z mapą taką, jak przed
uruchomieniem - niezapisane zmiany dalej niezapisane, `Ctrl+Z` cofa je jak przedtem -, tak samo menu gry
pod `Esc` (WYJDZ Z GRY → WROC DO EDYTORA albo TAK) i koniec poziomu (wygrana albo przegrana).

### 10. Ustawienia misji i teksty

Menu → **USTAWIENIA MISJI** (albo zębatka na listwie):

- **Tytuł** na stronie własnych map i **nagłówek odprawy** (na początku nazwa mapy wielkimi
  literami);
- **pieniądze na początku** - nowa mapa zaczyna z 0: daj graczowi trochę (np. 1000), inaczej fabryka
  niczego nie zbuduje; **pora dnia na początku** (dzień, wieczór, noc, ranek - potem płynie dalej,
  po 5 minut);
- **pogoda** - deszcz, śnieg, chmury, chmury burzowe albo meteoryty - i **jak często**: nigdy, od czasu
  do czasu (przychodzi i odchodzi, mniej więcej co pół minuty) albo cały czas;
- **błyskawice** - błyska przy czystym niebie; z deszczem tylko, gdy pada; przy śniegu, chmurach i
  meteorytach nie ma ich;
- **muzyka, grana po kolei** - lista plików z folderu `music` gry: wybierz plik z listy rozwijanej i
  **dodaj** go (za plikiem wybranym na liście albo na końcu), **usuń**, **w górę** i **w dół** zmieniają
  listę; poziom gra je od pierwszego, w kółko, jeden plik gra bez końca, a pusta lista to brak muzyki
  (`mus00` to muzyka zwycięstwa);
- **atakujących naraz** - ile pociągów wroga atakuje jednocześnie;
- **przyjazdy pociągów** - ich liczba i przycisk do ich panelu (krok 5); mapa ze starszej wersji ma tu
  **fale posiłków gracza** i **ich pociąg** (numery, np. `5,20`), dopóki nie zostanie przestawiona;
- **pociągi wroga na początku**, jeden w linii: `typ x y` (walczy stamtąd) albo `typ x1 y1 x2 y2`
  (patroluje między dwoma punktami toru). Typy to `light_`, `medium_`, `hard_`, `very_hard_`, `diff_`
  albo `defence_` z `gun`, `mortair`, `rocket`, `air`, `fire` albo `desant`, np. `light_gun`. Punkty są
  punktami ekranu: narzędzie **Linia jednokierunkowa** pokazuje w wierszu stanu punkt toru pod
  kursorem;
- **plan fabryki wroga**: ile ma pociągów; **Plan...** otwiera panel fabryki wroga, gdzie się je
  ustawia;
- **zadania**, teksty **zwycięstwa** i **porażki** oraz **odprawa** (linia `---` zaczyna następną
  stronę). Przy zapisie mapy są zapisywane wielkimi literami, po 44 znaki w linii.

Misja jest **wygrana**, gdy wróg miał jednostki i nie zostało mu żadnej (pociągi, fabryka, działa
fortyfikacji), a **przegrana**, gdy graczowi nie zostało żadnej jednostki ani fabryki.

### 11. Zagraj

Menu → **ZAGRAJ ZAPISANA MAPE**: gra uruchamia się od nowa z twoją zapisaną mapą jako misją - odprawa w menu Esc,
przybywające posiłki, atakujący wróg. Gdy się skończy, gra wraca do edytora na twoją mapę; żeby
wrócić od razu, naciśnij Esc, przycisk wyjścia (WYJDZ Z GRY) i **WROC DO EDYTORA**. O niezapisane
zmiany najpierw pyta.

### 12. Zapisz

`Ctrl+S` zapisuje mapę do jej pliku; mapa, która go jeszcze nie ma (mapa kampanii, nowa mapa), pyta
o nazwę - tak samo POLIGON i szablon, które aktualizacja by nadpisała (okno proponuje `poligon1`,
`map1` ..., a jego tytuł mówi dlaczego). Menu → **ZAPISZ MAPE** (Alt+F2) pyta zawsze - żeby zapisać pod inną nazwą. Mapa zapisana jako
`maps\<nazwa>\<nazwa>.map` zapisuje się ze skryptem i tekstami i pojawia na stronie **WLASNE MAPY**
pod swoim tytułem. Otworzysz ją znowu przez **WCZYTAJ MAPE** (Alt+F3). Każdy zapis pokazuje na chwilę
duże **ZAPISANO** z plikiem mapy na środku widoku, z kliknięciem menu.

### 13. Zapis, eksport i udostępnianie

**Eksport:** Menu → **EKSPORT MAPY** pakuje bieżącą mapę - mapę, jej pomalowany teren, skrypt, teksty i odprawę - do jednego pliku `.zip`; okno zapisu otwiera się na Pulpicie. Najpierw zapisz zmiany (edytor to proponuje). Map kampanii i pustego szablonu nie da się eksportować.

**Import:** Menu → **IMPORT MAPY** bierze taki `.zip` (także spakowany ponownie przez Windows) i dodaje mapę na stronę **WLASNE MAPY**. Gdy masz już mapę o tej nazwie, edytor pyta: **Tak** nadpisuje twoją, **Nie** zostawia obie (nowa dostaje nazwę `<nazwa>_2`), **Anuluj** przerywa. Potem proponuje otworzyć mapę w edytorze.

Mapa to jej folder: `maps\<nazwa>\` w folderze gry zawiera mapę (`<nazwa>.map`), jej pomalowany teren
(`<nazwa>.vid`), skrypt (`<nazwa>.lgc`) i teksty. Żeby dać ją znajomemu ręcznie: wyślij cały folder;
znajomy wkłada go do `maps\` swojego folderu gry i mapa jest na stronie własnych map. Folder
przemianowany w Eksploratorze też działa: gra sama zmienia nazwy plików w środku na nazwę folderu.

**Zmiana nazwy i usuwanie:** Menu → **ZMIEN NAZWE / USUN** daje mapie nową nazwę (jej folder i pliki
idą za nią, stary folder trafia do Kosza) albo ją usuwa (po pytaniu, do Kosza). Nazwa może mieć do 64
liter angielskich, cyfr, spacji i znaków dozwolonych w nazwach plików Windows i nie może być folderem
gry (`l01`-`l20`, `sandbox`, `template`, `script`, `outro`).

## 7. Klawisze i narzędzia edytora

**Pasek** (gniazda 1 - 0):

| Klawisz | Narzędzie | |
|---|---|---|
| 1 | Zaznacz | zaznaczanie, przeciąganie, obrót, armia, panel |
| 2 | Usun | kliknięcie usuwa; przeciągnięcie z wciśniętym przyciskiem usuwa wszystko po drodze (jeden krok cofania); Shift+klik na szynie: jej odcinek |
| 3 | Tory | kładzie szyny (V rodzaj, PgUp/PgDn albo kółko rampa, B podsypka) |
| 4 | Linia jednokierunkowa | linia na torze w jedną stronę |
| 5 | Fabryka | na ślepym końcu, z linią wyjazdu |
| 6 | Punkt naprawczy | na prostym odcinku, z linią |
| 7 | Fortyfikacja | na torze, uzbrojona |
| 8 / 9 | Semafor zielony / czerwony | obok toru |
| 0 | Wszystkie obiekty | paleta |

**Listwa** (nad paskiem, od lewej): Wysokosc terenu, Malowanie terenu, Woda i lawa, Region, Kopalnia,
Skrzynka z pieniedzmi, Pojazdy, puste miejsce, Przyjazdy pociagow, Ustawienia misji - narzędzia terenu,
rzadziej stawiane rzeczy, na końcu dwa panele.

**Klawisze:**

| Klawisz | |
|---|---|
| R / E, kółko | obrót: o krok kierunków obiektu (R w prawo, E w lewo); zawrócenie pociągu (E: tylko zaznaczonego wagonu), wraku na torze, semafora przy torze, a w narzędziu Linia jednokierunkowa linii; fabrykę i punkt naprawczy obraca ich tor (strona pomocy 15) |
| A | następna armia; przy stawianiu: armia następnego obiektu |
| T | semafor czerwony / zielony; na linii: pchanie tak / nie |
| L / Shift+L | linia przy zaznaczonym semaforze: wpuszcza / zatrzymuje |
| Del / Shift+Del | usuń / cały pociąg albo odcinek |
| Ctrl+Z / Ctrl+Y | cofnij / ponów |
| U / Shift+U / C | odczep przed wagonem / za nim, doczep go (do pociągu na jego torze) |
| `[` `]` | następny obiekt rzędu nad paskiem, tekstura |
| `<` `>` | odstęp dekoracji |
| kółko, `+` `-` | rozmiar pędzla (Wysokosc terenu, Dekoracje, Malowanie, Woda) |
| M | Wysokosc terenu: następny tryb (podnieś/obniż, wyrównaj, wygładź, rampa, nierówności) |
| H | wysokości terenu (poziomice) przy każdym narzędziu |
| B | podsypka nowych torów |
| V | rodzaj nowych szyn |
| PgUp / PgDn | zaznaczony obiekt wyżej / niżej o 4 (Shift: o 16); przy stawianiu: wysokość obiektu do postawienia; narzędzie Tory: następna szyna wznosi się / opada (także kółko) |
| Home | zaznaczony obiekt z powrotem na ziemi; przy stawianiu: obiekt do postawienia na ziemi |
| Ctrl+C / Ctrl+V / Ctrl+D | kopiuj zaznaczony obiekt / wstaw go pod wskaźnikiem / powiel obok oryginału |
| Ctrl+G | stawianie na siatce (jej rozmiar w rzędzie) |
| N | stawianie na linii pobliskiego toru |
| Q | stawianie: obrót jak widać albo losowy |
| X | stawianie: pojedynczo albo rozrzut przeciąganiem (dla wybranego obiektu) |
| G | narzędzie Tory: teren pod wznoszącymi się szynami bez zmian / podniesiony łagodnie / ostro |
| W | woda, bagno albo lawa |
| S | semafory zaznaczonej linii (narzędzie Linia jednokierunkowa) |
| Shift (z pędzlem) | usuwanie |
| Esc, prawy przycisk | powrót do Zaznacz; anulowanie zaczętego odcinka toru; zamknięcie panelu. Esc, gdy nic nie jest w toku: menu edytora |
| środkowy przycisk | przeciąganie mapy; w narzędziu Tory kliknięcie usuwa szynę pod kursorem (także na rozjeździe) |
| Ctrl+R | TEST BEZ ZAPISU: uruchom mapę tutaj / wróć do edytora |
| F1 | pomoc, od spisu treści |
| Ctrl+S | zapisz mapę |
| Alt+F2 / Alt+F3 | zapisz pod nazwą / wczytaj |

## 8. Pytania i problemy

- **Gra się nie uruchamia albo instalator nie znajduje Locolandu.** Locoland musi być zainstalowany
  przez Steam (w dowolnym języku).
- **Gdzie są moje mapy?** W `maps\` folderu gry (C:\Users\<ty>\Games\OpenLocoland), każda w swoim folderze;
  strona własnych map sama pokazuje każdą z nich (`maps\custom.lst` to własna lista gry).
- **Coś poszło nie tak.** Gra zapisuje w `logs\` swojego folderu `error.log` i `LocoFix.log`; wyślij je z
  opisem tego, co się stało.
- **Pełny ekran.** Działa, ale sprawdzony jest tryb okna; jeśli ekran zostaje czarny, odznacz „pełny
  ekran” w okienku startowym.

---

OpenLocoland jest projektem nieoficjalnym; Locoland należy do właścicieli praw (Gromada, ESDigital
Games, Alawar). Do grafiki na dzisiejszym Windowsie używa dgVoodoo2 autorstwa Dege.
