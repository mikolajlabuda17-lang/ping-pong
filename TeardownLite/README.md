# Teardown Lite (2D)

Lekka, 2D piaskownica destrukcji inspirowana Teardown. Działa na Linuksie, zależy od SDL2.

## Sterowanie
- Lewy przycisk myszy: niszczenie (eksplozja pędzla)
- Prawy przycisk myszy: budowanie (wypełnianie pędzla)
- [ i ]: zmiana rozmiaru pędzla
- H: pokaz/ukryj panel pomocy
- C: wyczyść planszę
- R: przywróć prosty teren (połowa ekranu)
- ESC: wyjście

## Budowanie
Wymagane pakiety: `libsdl2-dev` i kompilator C++17.

```bash
sudo apt update && sudo apt install -y build-essential libsdl2-dev
make
```

## Instalacja
- Kliknij dwukrotnie `installer.desktop` lub uruchom w terminalu:

```bash
bash -lc 'ROOT_DIR="$(pwd)"; bash ./install.sh "$ROOT_DIR"'
```

Skrypt stworzy folder `bin/` i przeniesie tam binarkę `teardown-lite`.

## Uruchomienie
```bash
cd bin
./teardown-lite
```

## Rozmiar
Projekt i binarka są lekkie (zależne od bibliotek systemowych), finalny ZIP << 30 MB.
