# OpenLocoland - przewodnik gracza

Wersja 0.1 - wydanie testowe. [English version: GUIDE.md](GUIDE.md)

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

Folder gry jest samodzielny: są w nim twoje mapy, tabele wyników i logi. Profil gracza (imię,
wygrane misje, wyniki, ustawienia dźwięku) jest zapisany w rejestrze twojego użytkownika Windows,
tak jak w oryginale.

**Aktualizacja.** Uruchom nowszy `Setup.exe` i wciśnij **Aktualizuj**: znajdzie zainstalowaną grę i skopiuje tylko to, co się zmieniło. Twoje mapy (w `maps\` folderu gry) i twój profil zostają.

**Odinstalowanie.** Uruchom `Setup.exe` i wciśnij **Odinstaluj**: usuwa skróty i folder gry ze wszystkim, co w nim jest (OpenLocoland nie figuruje w „Aplikacjach” Windows). Gra ze Steama zostaje bez zmian. Jeśli chcesz zachować swoje
mapy, skopiuj je najpierw (każda to folder w `maps\`, zob. [udostępnianie mapy](#13-zapis-eksport-i-udostępnianie)).

## 2. Pierwsze uruchomienie

Gra startuje okienkiem startowym (z obrazkiem Locolandu):

- **Rozdzielczość** - oprócz 640 x 480 ... 1024 x 768 lista ma tryby panoramiczne twojego monitora
  (od 1280 x 720 do 3840 x 2160) i rozdzielczość pulpitu.
- **Pełny ekran** - pole pod obrazkiem. Gdy nie jest zaznaczone, gra działa w **oknie** (okno
  pamięta swoje miejsce).
- **Język** - lista obok: języki, które masz (polski, angielski, ...). Napisy okienka zmieniają się
  razem z nim.
- **Limit FPS** i **Licznik FPS** - wiersz pod rozdzielczością: ile klatek na sekundę rysuje gra
  (domyślnie 60; ∞ = bez limitu) i mały licznik u góry ekranu.

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
- Pod nim są własne mapy: te dołączone do gry i te, które zrobisz albo dostaniesz od znajomych.
  Długa lista się przewija - strzałki obok niej, kółko myszy, klawisze w górę i w dół.
- Kliknięcie mapy pokazuje jej odprawę; **URUCHOM GRE** gra ją jak misję kampanii.

![POLIGON](img/poligon-pl.jpg)

**POLIGON** to gotowa misja zrobiona tymi samymi narzędziami co w edytorze: wróg zbudował bastion
na wschodzie - bramy strzegą dwa działa fortyfikacji, za nią stoi jego fabryka i punkt naprawczy, a
z fabryki co jakiś czas wyjeżdżają nowe pociągi. Przebij się i zniszcz je.

Własna mapa nie jest misją kampanii: jej wygranie nie odblokowuje misji, nie dodaje do wyniku
kampanii, stopnia ani galerii chwały. Ma własną tabelę wyników, „ponów misję” w menu Esc gra ją od
nowa, a po niej menu wraca na stronę własnych map.

## 5. Edytor map

Edytor to własny tryb edycji silnika, w którym autorzy robili swoje mapy i którego gra nigdy nie
udostępniła: co postawisz, to gra zagra. Otworzysz w nim każdą mapę - nową, POLIGON albo misję
kampanii (mapę kampanii zapisz pod własną nazwą, zanim zaczniesz ją zmieniać).

**Uruchamianie:** **EDYTOR MAP** na stronie własnych map - gra uruchamia się od nowa w edytorze, na
pustej mapie-szablonie. Skrót na pulpicie **Locoland - edytor map** otwiera go od razu.

![Edytor](img/editor-factory-pl.jpg)

Co widać:

- **Mapę** tak, jak ją zrobiono, w bezruchu: poziom jest *wstrzymany* - nic się nie rusza, żaden
  skrypt nie działa. Poza mapą jest przyciemniona siatka, a krawędź mapy ma ramkę (można przewinąć
  300 px poza krawędź: tory biegną tam dalej).
- **Pasek** na dole w wyglądzie paska stanu gry: narzędzia 1 - 0 w jego gniazdach, kolejne na
  **listwie** nad nim. Wybrane jest w ramce.
- **Wiersz stanu** u góry: narzędzie (albo to pod myszą), liczba kroków do cofnięcia i ponowienia,
  „nie zapisano” i ostatni komunikat - dlaczego czegoś nie da się zrobić, zawsze jest napisane tutaj.
- **Przyciski ? i menu** na lewym końcu paska (tam, gdzie gra ma przycisk pauzy). **?** (albo F1)
  otwiera pomoc: dziesięć stron z tym, co jest w tym rozdziale, w skrócie. **Przycisk menu**
  otwiera menu edytora ze wszystkimi poleceniami - nowa mapa, wczytaj, zapisz, ustawienia misji,
  rozmiar mapy, zagraj na mapie, cofnij, ponów, uruchom poziom, powrót do menu, wyjdź z gry.

  ![Pomoc](img/editor-help-pl.jpg)

  ![Menu edytora](img/editor-menu-pl.jpg)

**Poruszanie się:** krawędź ekranu, klawisze strzałek albo przeciąganie środkowym przyciskiem myszy.

**Podstawy edycji** (narzędzie Zaznacz, klawisz 1):

| Czynność | Jak |
|---|---|
| zaznaczenie | kliknij obiekt (najmniejszy pod kursorem); oznacza go kółko |
| przesunięcie | przeciągnij go |
| obrót | R / E albo kółko myszy |
| zmiana armii | A: nasi, wróg, neutralni, piraci - flagi zmieniają kolor |
| usunięcie | Del (Shift+Del: cały pociąg; Shift+klik narzędziem Usun: cały odcinek toru) |
| cofnij / ponów | Ctrl+Z / Ctrl+Y - każdą zmianę, ile kroków chcesz |
| panel obiektu | prawy przycisk myszy na nim |

**Panel obiektu** (prawy przycisk) ma jego armię, obrót i usuwanie, kolor semafora i linię przy
nim, a dla fortyfikacji, kopalni i cytadeli - działo w każdym gnieździe (puste, działo albo
rakiety). Prawy przycisk gdzie indziej go zamyka.

![Panel fortyfikacji](img/editor-fort-panel-pl.jpg)

## 6. Samouczek: twoja pierwsza misja

Zrobimy małą misję: pociąg gracza przyjeżdża zza zachodniej krawędzi, gracz ma fabrykę i punkt
naprawczy, wróg ma na linii fortyfikację z dwoma działami i patrolujący pociąg. Misja jest wygrana,
gdy wrogowi nie zostanie żadna jednostka.

### 1. Nowa mapa

Przycisk menu → **NOWA MAPA**:

![Okno Nowa mapa](img/new-map-pl.jpg)

- **Folder** - nazwa mapy: małe litery, cyfry i `_` (np. `most_1`). Mapa to `maps\most_1\most_1.map`.
- **Teren** - *pusta mapa* z jednej tekstury (trawa, piasek, śnieg, skała ... - płaska i goła, do
  malowania), *łąka* albo teren jednej z map kampanii.
- **Rozmiar** - dla pustej mapy albo łąki: od 512 do 4096 px w każdą stronę co 256 (`-` i `+`) albo
  gotowy: 1024 x 768 (mała, jak misja 1), 2048 x 1536 (większość misji), 3072 x 2304 (duża). Na
  początek weź 2048 x 1536.

OK - edytor otwiera nową mapę. Ma już skrypt misji i teksty zrobione z szablonu; uzupełnisz je w
kroku 10. (**ROZMIAR MAPY** w menu zmienia rozmiar później.)

### 2. Pomaluj teren

**Malowanie terenu** z listwy: `[` i `]` wybierają teksturę (trawa, jasna trawa, ściółka, piasek,
pustynia, śnieg, lód, skała, ciemna skała, lawa, bagno, dno jeziora - wzięte z map kampanii), `+` i
`-` pędzel (8 - 200 px). Maluj przeciągając. Shift+przeciąganie przywraca oryginalny obraz. Namaluj
piaszczystą drogę tam, gdzie pobiegną tory, i kilka plam innych tekstur. Pomalowany obraz zapisuje
się razem z mapą.

### 3. Woda, bagno, lawa

**Woda i lawa** z listwy: `W` wybiera wodę, bagno albo lawę; przeciągaj, żeby ją położyć. Obniża teren
w nieckę z brzegiem, kładzie powierzchnię wody i dodaje jej efekty (kręgi na wodzie, bąble, wybuchy
lawy) oraz region, w którym wybuchy rozpryskują wodę. Shift+przeciąganie ją usuwa. Jeden rodzaj
wody ma na mapie jeden poziom, tak jak w misjach.

Narzędzie **Wysokosc terenu** podnosi (2 na krok), obniża, wyrównuje albo wygładza teren pod
przeciąganiem (`M` pędzel, `+`/`-` promień): na tych wysokościach stoją obiekty. Torów nie rusza.

### 4. Główna linia

**Tory** (klawisz 3). Kliknij, żeby zacząć linię - na końcu szyny mapy albo gdziekolwiek - a każde
następne kliknięcie kładzie **jedną szynę** w stronę kursora; prawy przycisk kończy linię. Podgląd
pokazuje następną szynę: **zielone** końce - pasuje, **czerwone** - wiersz stanu mówi, którą
zasadę łamie:

- krok to 71 x 50 px albo jego połowa (siatka torów);
- skręt zawsze przez **łuk**, najwyżej o 45° (prosta szyna nie łączy się wprost ze skośną);
- w jednym punkcie spotyka się najwyżej 6 szyn;
- **rozjazd**: zacznij nową linię w punkcie istniejącej - odgałęzienie to prosta szyna i łuk o tym
  samym środku, **rozjazd Y** jak w misjach.

Główną linię połóż tak, jak robią to misje: **zza zachodniej krawędzi** (przewiń nad siatkę) przez
mapę, żeby pociągi przyjeżdżały i odjeżdżały poza widokiem. Potem bocznica na fabrykę: odgałęzienie
od linii przez Y, prosty odcinek kończący się **ślepym końcem**.

`V` zmienia rodzaj następnych szyn: zwykłe, bagienne (na nasypie), zniszczone albo zniszczone
bagienne. `PgUp` / `PgDn` sprawiają, że następna szyna wznosi się albo opada (rampa na wyższy
teren). `B` wybiera podsypkę pod nowe szyny. Del albo narzędzie **Usun odcinek toru** usuwa szyny.

### 5. Punkt przybycia posiłków

**Punkt przybycia posilkow** (klawisz 0): kliknij na głównej linii **za zachodnią krawędzią**. Tu
na początku misji przyjeżdża pociąg gracza (a później kolejne fale, zob. krok 10). Sam staje na
najbliższym torze.

### 6. Fabryka, punkt naprawczy, linie jednokierunkowe i semafory

**Fabryka** (klawisz 4) i **Punkt naprawczy** (klawisz 5) stają tam, gdzie stawiali je autorzy -
podgląd pokazuje gdzie, a postawienie gdzie indziej jest odrzucane z podaniem powodu:

- **fabryka** na **ślepym końcu bocznicy**, obrócona wyjazdem na zewnątrz. Dostaje swoją **linię
  wyjazdu**: linię jednokierunkową wzdłuż bocznicy (zbudowane wagony pojawiają się w fabryce i są
  wypychane) i czerwony semafor przy niej;
- **punkt naprawczy** na **prostym odcinku** toru, obrócony wzdłuż niego jak brama. Dostaje linię
  jednokierunkową przez siebie (pociągi są przez niego przepychane, a każdy wagon naprawiany) i
  swoje semafory.

Przeciągnięcie przenosi je razem z linią na najbliższe właściwe miejsce; usunięcie usuwa też linię.

**Linie jednokierunkowe** (**Linia jednokierunkowa** z listwy): linia na torze, na którą żaden
pociąg nie wjedzie pod prąd - tak misje oznaczają wjazd na mapę, wyjazd, fabryki i punkty naprawcze.
Kliknij jej początek i koniec na torze; potem na zaznaczonej linii `T` włącza albo wyłącza
pchanie, `R` ją odwraca, `S` stawia jej semafory, Del ją usuwa. Zrób dwie:

- **wjazd na mapę**: od końca głównej linii za krawędzią na mapę, z pchaniem;
- **wyjazd z mapy** na drugim końcu, krótki i skierowany w głąb mapy, bez pchania - żeby żaden
  pociąg nie wyjechał poza mapę.

**Semafory** (klawisze 8 zielony, 9 czerwony) są tylko znakami - pokazują kierunek linii
jednokierunkowych i stoją obok toru; `T` zmienia kolor. Na zaznaczonym semaforze `L` robi przy nim
linię, która wpuszcza pociągi, na które patrzy (zielony), a `Shift+L` taką, która je zatrzymuje
(czerwony).

### 7. Wróg

- **Fortyfikacja** (klawisz 6, wroga): postaw ją **na torze** głównej linii - to brama, przez którą
  przejeżdżają pociągi. Jest od razu uzbrojona: jej panel (prawy przycisk) ustawia działo w każdym
  gnieździe - działo albo rakiety. Samej fortyfikacji nie da się zniszczyć; jej działa tak.
- **Wszystkie obiekty** (klawisz 7) otwiera paletę ze wszystkimi obiektami z misji, według tematów
  (łąka, las, pustynia, lód, wulkan, bagno, park, piraci, miasto, przemysł, kolej, flagi i skrzynie,
  wraki, jednostki i stworzenia, efekty): kliknij jeden, potem postaw go na mapie. Są tu **kopalnia**
  (dwa gniazda na działa, jak fortyfikacja), cytadela, mury, budynki, roboty.

  ![Wszystkie obiekty](img/editor-palette-pl.jpg)

- **Pociągi**: **Pojazdy** z listwy otwierają wagony - lokomotywy, wagony bojowe, zaopatrzenie
  (wóz z amunicją, pojazd naprawczy, cysterny), wagony cywilne, przeszkody. Wagon staje na torze
  pod kursorem; postawiony na końcu pociągu dołącza się do niego. Zrób pociąg wroga: lokomotywa i
  działo, potem `A` na nim dla armii wroga. `U` odczepia wagon, `C` doczepia do innego pociągu, `R`
  odwraca pociąg, przeciąganie przesuwa go po torze. Pociąg bez **lokomotywy** nie pojedzie.
- **Patrol**: panel pociągu → PATROL → do punktu..., potem kliknij drugi koniec na torze. Pociąg
  jeździ tam i z powrotem.
- Więcej pociągów wroga ustawisz w ustawieniach misji (krok 10): pociągi na początku i pociągi,
  które jego fabryka wysyła później. Do tych drugich wróg potrzebuje fabryki: postaw ją (klawisz 4)
  na własnym ślepym końcu i `A` na wroga.
- **Skrzynka z pieniedzmi** (listwa) leży na torze: jednostka gracza, która ją zbierze, dostaje 100.

Pamiętaj o zasadzie gry: fabryka, punkt naprawczy i fortyfikacja stoją **na torze albo tuż przy
nim**.

### 8. Ozdoby

**Dekoracje** z listwy: trawa, krzaki, kwiaty, kamienie, wraki... `[` `]` (albo kliknięcie pozycji)
wybiera jedną; kliknięcie stawia jedną, przeciąganie rozrzuca je w promieniu (`+` `-`), nigdy
bliżej niż odstęp (`<` `>`). Shift+przeciąganie je zbiera. Drzewa, domy i reszta są we **Wszystkie
obiekty**. **Region** (listwa): przeciągnij jego prostokąt; jego panel ustawia mgłę (mgła, pył,
opar, poświata) i rozpryski.

### 9. Zobacz, jak działa

Menu → **URUCHOM POZIOM** (albo `Ctrl+R`): poziom ożywa w edytorze - pociągi jeżdżą, działa
strzelają. `Ctrl+R` znowu go wstrzymuje. (Poziom, który działał, nie jest już taki, jak go
postawiłeś: jeśli coś zmieniało się w czasie działania, wczytaj go od nowa przed zapisem.)

### 10. Ustawienia misji i teksty

Menu → **USTAWIENIA MISJI** (albo zębatka na listwie):

- **Tytuł** na stronie własnych map i **nagłówek odprawy**;
- **pieniądze** na początku, **pora startu** (dzień, wieczór, noc, ranek), **deszcz**, **błyskawice**;
- **atakujących naraz** - ile pociągów wroga atakuje jednocześnie;
- **fale posiłków gracza** i **ich pociąg** (rodzaje wagonów jako numery, np. `5,20` - lokomotywa i
  działo; numery są w palecie Pojazdy obok nazw). Pierwsza fala przywozi pociąg gracza na początku;
  następna przychodzi, gdy pociągu gracza już nie ma, a fabryka stoi;
- **pociągi wroga na początku**, jeden w linii: `typ x y` (walczy stamtąd) albo `typ x1 y1 x2 y2`
  (patroluje między dwoma punktami toru). Typy to `light_`, `medium_`, `hard_`, `very_hard_`, `diff_`
  albo `defence_` z `gun`, `mortair`, `rocket`, `air`, `fire` albo `desant`, np. `light_gun`. Punkty są
  punktami ekranu: narzędzie **Linia jednokierunkowa** pokazuje w wierszu stanu punkt toru pod
  kursorem;
- **pociągi wroga z jego fabryki**: `sekundy typ` (albo z patrolem `sekundy typ x1 y1 x2 y2`) -
  najwcześniejszy czas zbudowania;
- **zadania**, teksty **zwycięstwa** i **porażki** oraz **odprawa** (linia `---` zaczyna następną
  stronę). Przy zapisie mapy są zapisywane wielkimi literami, po 44 znaki w linii.

Misja jest **wygrana**, gdy wróg miał jednostki i nie zostało mu żadnej (pociągi, fabryka, działa
fortyfikacji), a **przegrana**, gdy graczowi nie zostało żadnej jednostki ani fabryki.

### 11. Zagraj

Menu → **ZAGRAJ NA MAPIE**: gra uruchamia się od nowa z twoją mapą jako misją - odprawa w menu Esc,
przybywające posiłki, atakujący wróg. Gdy się skończy (albo wyjdziesz do menu głównego), gra wraca
do edytora na twoją mapę. O niezapisane zmiany najpierw pyta.

### 12. Zapisz

Menu → **ZAPISZ MAPE** (Alt+F2). Mapa zapisana jako `maps\<nazwa>\<nazwa>.map` -
nowa mapa już tam jest - zapisuje się ze skryptem i tekstami i pojawia na stronie **WLASNE MAPY**
pod swoim tytułem. Otworzysz ją znowu przez **WCZYTAJ MAPE** (Alt+F3).

### 13. Zapis, eksport i udostępnianie

**Eksport:** Menu → **EKSPORT MAPY** pakuje bieżącą mapę - mapę, jej pomalowany teren, skrypt, teksty i odprawę - do jednego pliku `.zip`; okno zapisu otwiera się na Pulpicie. Najpierw zapisz zmiany (edytor to proponuje). Map kampanii i pustego szablonu nie da się eksportować.

**Import:** Menu → **IMPORT MAPY** bierze taki `.zip` (także spakowany ponownie przez Windows) i dodaje mapę na stronę **WLASNE MAPY**. Gdy masz już mapę o tej nazwie, edytor pyta: **Tak** nadpisuje twoją, **Nie** zostawia obie (nowa dostaje nazwę `<nazwa>_2`), **Anuluj** przerywa. Potem proponuje otworzyć mapę w edytorze.

Mapa to jej folder: `maps\<nazwa>\` w folderze gry zawiera mapę (`<nazwa>.map`), jej pomalowany teren
(`<nazwa>.vid`), skrypt (`<nazwa>.lgc`) i teksty. Żeby dać ją znajomemu ręcznie: wyślij cały folder;
znajomy wkłada go do `maps\` swojego folderu gry i dopisuje na końcu `maps\custom.lst` (Notatnik)
dwa wiersze: nazwę folderu i tytuł wielkimi literami, np.

```
most_1
MOST BEZ POWROTU
```

## 7. Klawisze i narzędzia edytora

**Pasek** (gniazda 1 - 0):

| Klawisz | Narzędzie | |
|---|---|---|
| 1 | Zaznacz | zaznaczanie, przeciąganie, obrót, armia, panel |
| 2 | Usun | kliknięcie usuwa; Shift+klik na szynie: jej odcinek |
| 3 | Tory | kładzie szyny (V rodzaj, PgUp/PgDn rampa, B podsypka) |
| 4 | Fabryka | na ślepym końcu, z linią wyjazdu |
| 5 | Punkt naprawczy | na prostym odcinku, z linią |
| 6 | Fortyfikacja | na torze, uzbrojona |
| 7 | Wszystkie obiekty | paleta |
| 8 / 9 | Semafor zielony / czerwony | obok toru |
| 0 | Punkt przybycia posilkow | tu przyjeżdża pociąg gracza |

**Listwa** (nad paskiem, od lewej): Usun odcinek toru, Dekoracje, Linia jednokierunkowa, Ustawienia
misji, Wysokosc terenu, Skrzynka z pieniedzmi, Pojazdy, Malowanie terenu, Woda i lawa, Region.

**Klawisze:**

| Klawisz | |
|---|---|
| R / E, kółko | obrót (pociąg: odwrócenie) |
| A | następna armia |
| T | semafor czerwony / zielony; na linii: pchanie tak / nie |
| L / Shift+L | linia przy zaznaczonym semaforze: wpuszcza / zatrzymuje |
| Del / Shift+Del | usuń / cały pociąg albo odcinek |
| Ctrl+Z / Ctrl+Y | cofnij / ponów |
| U / C | odczep / doczep wagon |
| `[` `]` | dekoracja, tekstura |
| `<` `>` | odstęp dekoracji |
| `+` `-` | promień pędzla |
| M | pędzel terenu: podnieś, obniż, wyrównaj, wygładź |
| B | podsypka nowych torów |
| V | rodzaj nowych szyn |
| PgUp / PgDn | następna szyna wznosi się / opada |
| W | woda, bagno albo lawa |
| S | semafory zaznaczonej linii (narzędzie Linia jednokierunkowa) |
| Shift (z pędzlem) | usuwanie |
| Esc, prawy przycisk | powrót do Zaznacz; koniec linii toru |
| Ctrl+R | uruchom / wstrzymaj poziom |
| F1 | pomoc |
| Alt+F2 / Alt+F3 | zapisz / wczytaj |

## 8. Pytania i problemy

- **Gra się nie uruchamia albo instalator nie znajduje Locolandu.** Locoland musi być zainstalowany
  przez Steam (w dowolnym języku).
- **Gdzie są moje mapy?** W `maps\` folderu gry (C:\Users\<ty>\Games\OpenLocoland), każda w swoim folderze; lista
  strony własnych map to `maps\custom.lst`.
- **Coś poszło nie tak.** Gra zapisuje w swoim folderze `error.log` i `LocoFix.log`; wyślij je z
  opisem tego, co się stało.
- **Pełny ekran.** Działa, ale sprawdzony jest tryb okna; jeśli ekran zostaje czarny, odznacz „pełny
  ekran” w okienku startowym.

---

OpenLocoland jest projektem nieoficjalnym; Locoland należy do właścicieli praw (Gromada, ESDigital
Games, Alawar). Do grafiki na dzisiejszym Windowsie używa dgVoodoo2 autorstwa Dege.
