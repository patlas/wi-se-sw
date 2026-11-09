# Rozmowa o Wi-Se — od pytania o język programowania


## Asystent

Projekt **Wi-Se na ESP8266 jest napisany głównie w C++**, z elementami C.

W projekcie wykorzystano kilka języków:

| Język | Zastosowanie |
|---|---|
| **C / C++** | Firmware ESP8266, UART, Wi-Fi, WebSocket, HTTP |
| **Python** | Skrypty budowania i konfiguracji (`builder.py`) |
| **TypeScript / JavaScript** | Interfejs terminala w przeglądarce |
| **HTML / CSS** | Wygląd interfejsu WWW |
| **YAML** | Konfiguracja urządzenia przed kompilacją |

### Struktura projektu

Repozytorium: https://github.com/Depau/wi-se-sw

```text
wi-se-sw/
├── src/          # Firmware ESP8266 (C/C++)
├── include/      # Pliki nagłówkowe
├── html/         # Interfejs WWW
├── configs/      # Konfiguracja YAML
├── builder/      # Narzędzia budowania
└── builder.py    # Skrypt Python
```

### Co to oznacza dla Twojego ESP-01S?

Jeśli chcesz dodać funkcjonalność, o której wcześniej rozmawialiśmy, czyli:

1. ESP-01S startuje jako Access Point.
2. Użytkownik otwiera stronę konfiguracyjną.
3. Wybiera sieć Wi-Fi i wpisuje hasło.
4. ESP8266 zapisuje konfigurację.
5. Urządzenie przełącza się do trybu STA.

To **większość zmian trzeba będzie napisać w C/C++**, a formularz konfiguracyjny w TypeScript/HTML.

Warto wiedzieć, że dokumentacja Wi-Se wskazuje, że zmiany konfiguracji podczas działania nie są obsługiwane. Taki mechanizm wymaga więc modyfikacji istniejącej architektury.

**Dobra wiadomość:** nie musimy przepisywać całego Wi-Se. Możemy zachować istniejący terminal UART/WebSocket i rozszerzyć tylko obsługę Wi-Fi oraz konfiguracji.

Największym wyzwaniem będzie zmieszczenie całości w **1 MB Flash Twojego ESP-01S**.

---

## Użytkownik

co jest potrzebne aby skompilować projekt?

## Asystent

Do skompilowania **Wi-Se dla ESP8266 (ESP-01S)** potrzebujesz przede wszystkim **Pythona, PlatformIO, kompilatora C/C++ dla ESP8266 oraz kodu źródłowego projektu**.

Na Linuxie nie musisz ręcznie instalować kompilatora dla ESP8266 — PlatformIO pobierze odpowiedni toolchain i zależności.

### 1. Wymagane narzędzia

| Narzędzie | Zastosowanie |
|---|---|
| Git | Pobranie kodu Wi-Se |
| Python 3 | Uruchamianie skryptów budowania |
| PlatformIO | Kompilacja firmware |
| GCC dla Xtensa | Kompilator ESP8266, pobierany przez PlatformIO |
| pip / venv | Instalacja zależności Pythona |
| esptool | Wgrywanie firmware na ESP8266 |

Do samej kompilacji **nie potrzebujesz podłączonego ESP-01S ani programatora USB**.

### 2. Przygotowanie środowiska na Ubuntu/Debian

Zainstaluj podstawowe pakiety:

```bash
sudo apt update

sudo apt install -y \
  git \
  python3 \
  python3-pip \
  python3-venv \
  python3-dev \
  build-essential
```

Utwórz środowisko wirtualne:

```bash
mkdir -p ~/esp-projects
cd ~/esp-projects

python3 -m venv .venv
source .venv/bin/activate
```

Zainstaluj PlatformIO:

```bash
pip install platformio esptool
```

Sprawdź instalację:

```bash
pio --version
python -m esptool version
```

### 3. Pobierz kod i zależności Wi-Se

```bash
cd ~/esp-projects

git clone https://github.com/Depau/wi-se-sw.git
cd wi-se-sw

pip install -r requirements.txt
```

Oficjalna dokumentacja wymaga Pythona 3.8+, PlatformIO Core oraz bibliotek `pyjq`, `jinja2` i `pyyaml`. Projekt jest testowany na Linuxie x86-64.

### 4. Przygotuj konfigurację

```bash
cp configs/config.yml.example configs/esp01s.yml
nano configs/esp01s.yml
```

Konfiguracja musi uwzględniać Twój ESP-01S, pamięć Flash 1 MB i wybrany tryb Wi-Fi.

Skrypt `builder.py` automatycznie generuje pliki `platformio.ini` i `include/config.h` na podstawie YAML.

### 5. Kompilacja firmware

Dokumentacja pokazuje polecenie:

```bash
./builder.py upload esp01s
```

**Uwaga:** `upload` również wgrywa firmware na ESP8266. Nie uruchamiaj go, dopóki nie potwierdzimy poprawności konfiguracji i rozmiaru obrazu.

Do samego budowania możemy wykorzystać wygenerowany projekt PlatformIO, ale najpierw trzeba sprawdzić opcje dostępne w Twojej wersji skryptu:

```bash
python builder.py --help
```

### 6. Czy potrzebujesz Node.js?

**Nie do zwykłej kompilacji firmware**, jeżeli korzystasz z gotowego interfejsu WWW znajdującego się w projekcie.

Jeżeli zamierzasz zmieniać stronę terminala, potrzebujesz dodatkowo Node.js i Yarn. Dokumentacja wskazuje Node.js 24.6.0, a interfejs buduje się poleceniem:

```bash
cd html
yarn install
yarn run build
```

To generuje `src/html.h`, zawierający interfejs WWW do osadzenia w firmware.

### Co proponuję teraz?

Dla Twojego **ESP-01S z 1 MB Flash** najpierw spróbowałbym skompilować oryginalne Wi-Se, bez modyfikacji, i sprawdzić rozmiar wynikowego firmware.

Wykonaj:

```bash
cd ~/esp-projects/wi-se-sw
python builder.py --help
```

Wklej wynik. Ustalimy właściwe polecenie kompilacji bez flashowania i sprawdzimy, czy Wi-Se zmieści się w 1 MB.

---
