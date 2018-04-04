# analog-servopoint

Analog-servopoint je zařízení určené k ovládání servo a spínaných (zap/vyp) výstupů pro analogově řízený model železnice. Základní deska má tyto vstupy a výstupy:

1. Vstup z maticové klávesnice 4x4 tlačítek. Funguje jak pro spínací mžiková tlačítka, tak pro přepínací tlačítka. U přepínacích tlačítek může mít význam přepnutí do obou poloh.
2. Ovládání až 8 modelářských serv. 
3. 8 tranzistorem spínaných výstupů, zatížení cca 500mA/výstup, celkem 2,5A

Software poskytuje následující funkce:

### Ovládání serva
1. Určení 2 poloh (levá, pravá) pro každé servo.
2. Určení rychlosti přestavování serva.
3. Interaktivní kalibrace serva
4. Plynulé přestavení serva do předdefinované pravé/levé polohy, nebo na libovolný úhel
5. Postupné spínaní serv, eliminaci proudové špičky, omezení zákmitu serva
6. "Ohlas" polohy serva na definovaném spínaném výstupu (ON/OFF)
7. Přestavování 2 serv najednou

### Ovládání jednotlivých výstupů
1. Zapnutí, vypnutí
2. Pulz 300ms, nebo zadané délky
3. (stále ve vývoji) Blikání s určenou dobou zapnutí a prodlevy. Blikání v protifázi.
4. Napájení tranzistorového pole lze volit jumperem, +5V nebo napětí zdroje.

### Reakce na tlačítka
Tlačítka se čtou po sloupcích, v případě přepinačů je nutné zařadit do matice diody.
1. Přiřazení sekvencí pokynů pro serva a spínané výstupy (max 10 akcí)
2. Spuštění sekvence při stisku, rozepnutí tlačítka nebo obojím
3. Vyčkání přestavení serva, nebo provedení několika akcí najednou
4. Při rozepnutí tlačítka je možné celou posloupnost akcí vrátit nebo přerušit
5. Volitelné prodlevy mezi akcemi
6. Postupné vykonání akcí pro tlačítka stisknutá najednou či v těsném sledu (max 20)

### Nastavení, komunikace
1. Deska se nastavuje a může komunikovat pomocí mikroUSB rozhraní Arduina. 
2. Je možné přímo přikazovat jednotlivé akce (spínání, nastavení serva)
3. Jednoduchý textový výpis konfigurace pro zálohování
4. Upgrade obslužného SW - pomocí Arduino IDE


Podrobnosti viz [Wiki](https://github.com/svatoun/analog-servopoint/wiki/)




