# con64OS

## Wprowadzenie

Projekt ten jest początkiem prac nad hobbystycznym jądrem systemu opercyjnego dla architektury x86_64 z firmwarem UEFI. Do bootowania wykorzystuje protokół BOOTBOOT (zdefiniowany przez autora: https://gitlab.com/bztsrc/bootboot). W żadnym wypadku nie jest to używalny lub stabilny projekt, opierający się na wielu założeniach (trochę nierealistycznych).

Aktualnie zaimplementowane jest podstawowe wsparcie dla układu zegarowego HPET, układów obsługi przerwań APIC i IOAPIC, bardzo podstawowe wsparcie dla dysków twardych podłączonych magistralą SATA do kontrolerów AHCI (możliwość odczytywania danych z medium, brak wsparcia dla zapisu), prosty sterownik dla portu szeregowego (tylko do zapisu, brak możliwości odczytu danych z portu szeregowego). Dodatkowo, w oparciu o dostarczany przez bootloader bufor ekranu, zaimplementowany jest prosty "terminal" renderowany w trybie graficznym z użyciem czcionki monospace 8x8.

Dokumentacja projektu w języku angielskim dostępna jest w folderze `Documentation/`. Dołączony jest zaktualizowany do stanu obecnego schemat klas oraz wygenerowana Doxygenem dokumentacja funkcjonalna zawartych klas.

## Instrukcja kompilacji

Do kompilacji projektu wymagany jest system linuxowy (być może da się go również skompilować w WSL2 pod Windowsem, nie testowałem tego). Dodatkowo (ze względów wygody, niekoniecznie najlepsze rozwiązanie) wymagany jest dostęp root'a (do wykorzystania loopback devices oraz polecenia `mount`).

Do kompilacji plików źródłowych wymagany jest cross-compiler `x86_64-elf-g++` w wersji co najmniej 10.3 (ze względu na wsparcie standardu C++20) oraz odpowiadający pakiet `binutils` (instrukcje kompilacji można znaleźć tu: https://wiki.osdev.org/GCC_Cross-Compiler)

Wymagane są również standardowe pakiety `make`, `dd`, `gdisk`, `mkdosfs` oraz `qemu-system` (w szczególności `qemu-system-x86_64`) do emulacji systemu. 

Gdy wszystkie zależności zostaną spełnione, wystarczy jedynie jako root, w folderze głównym projektu wykonać polecenie:

`make run` (`sudo make run` jeśli `sudo` jest zainstalowane)

## Instrukcja użytkowania

Ze względu na bardzo wczesną fazę rozwoju projektu, nie jest on interaktywny. Stosowane parametry `qemu` powodują przekierowanie portu szeregowego na `stdio` w momencie uruchomienia. Uruchamiający się kernel wypisze wówczas sporo informacji debuggowych i zatrzyma się w pętli nieskończonej. Zainicjalizowany zostanie również terminal graficzny, na który zostanie wypisany komunikat startowy. 

## Krótki opis modułów

* BootBoot/ - fragment projektu BOOTBOOT, dostosowany do potrzeb projektu
* Build/ - folder zawierający wygenerowany obraz dysku i potrzebne pliki binarne
* Documentation/ - dokumentacja w języku angielskim, wspomniana wcześniej
* Kernel/
  * driver/
    * acpi/ - moduł zawiera podstawowe wsparcie dla tablic ACPI dostarczonych przez firmware systemu
    * ahci/ - moduł zawiera bardzo podstawowe wsparcie dla kontrolera AHCI (ze wsparciem odczytu z dysków twardych)
    * arch/
      * apic.cpp/h - wsparcie dla kontrolerów przerwań APIC i IOAPIC (włącznie z ich enumeracją z tablicy ACPI)
      * cpu.cpp/h - moduł pozwalający na niskopoziomową kontrolę procesora
      * gdt.cpp/h - moduł umożliwiający zarządzaniem tablicą segmentów procesora
      * hpet.cpp/h - moduł wsparcia dla układu zegarowego HPET (na razie, wyłącznie ze wsparciem dla trybu one-shot)
      * ints.cpp/h - moduł zarządzający dla przerwać procesora, zajmuje się przydzielaniem wektorów i wywoływaniem odpowiednich procedur obsługi przerwań
      * portio.cpp/h - moduł pozwalający na komunikację z urządzeniami podłączonymi do portów IO procesora x86
    * bus/pcie/ - moduł zawiera kod enumerujący urządzenia podłączone do szyny PCIe w systemie
    * iface/ - moduł zawierający wspólne interfejsy komunikacyjne dla kilku części systemu (np. `ITextOutput` to protokół dla urządzeń wyjściowych dla tekstu)
    * text/
      * graphicsterm.cpp/h - bardzo prosty moduł zawierający wsparcie dla graficznego terminala
      * serial.cpp/h - prosty sterownik portu szeregowego (wyłącznie do zapisu)
  * mem/
    * heap.cpp/h - moduł zajmujący się dynamicznym przydzielaniem fragmentów pamięci do zastosowań kernela
    * physalloc.cpp/h - alokator pamięci fizycznej, potrafi alokować pamięć w stronach 4KiB oraz 2MiB
    * vas.cpp/h - bardzo prosty moduł zarządzający wirtualną przestrzenią adresową procesora, na razie bez wsparcia dla stron w przestrzeni użytkownika
  * util/
    * bootboot.h - moduł zawierający definicje potrzebne do korzystania z protokołu BOOTBOOT
    * critical.cpp/h - nieużywany moduł, pozwalający na tworzenie scope-limited sekcji krytycznych kodu
    * list.h - prosta implementacja generycznej listy
    * logger.cpp/h - implementacja prostego loggera w oparciu o szablony C++
    * spinlock.cpp/h - bardzo prosta implementacja spinlock'a (oraz mechanizmu blokowania ich w konkretnych scope'ach)
    * timer.cpp/h - prosta implementacja timera, potrafi czekać synchronicznie i asynchronicznie (z wykorzystaniem układu HPET)
    * types.h - deklaracja używanych w całym systemie typów




