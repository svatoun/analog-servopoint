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
1. Definování sekvencí pokynů pro serva a spínané výstupy (max 10 akcí)
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


## Rozšiřující desky
Základní deska je navržena jako *rozšiřitelná*. Je možné doplnit rozšiřující desky (viz dále), které zvětší počet vstupů, výstupů:
1. Rozšíření o 8 a 16 spínaných výstupů. Základní deska má vyveden konektor na kterém lze řetězit posuvné registry. Obslužný SW počítá s max 64 výstupy.
2. Rozšíření o 8 servo. Na základní desce je vyvedený konektor pro napájení servo, a konektor pro řídící signály a napájení elektroniky. Pomocná deska by měla obsahovat demultiplexor pro řídící signál. Postupné spínání napájení a práci s demultiplexory již obsahuje základní SW. Při rozšíření lze najednou přestavovat až 4 serva.
3. Rozšíření vstupu. Sloupce může obsluhovat posuvný registr; řádků může být 5, tedy lze připojit 5xN tlačítek v matici. Nebo naopak může pomocná deska obsahovat střadač a vstupy mohou být bez použití maticového uspořádání, to ale vyžaduje drobnou úpravu SW.
4. Rizení RS485. Deska má vyvedeny na konektor piny Arduina 2, 3, které mohou generovat přerušení, a dají se použít pro sériovou komunikaci. V zarušeném prostředí nebo na větší vzdálenost může nahradit vstup maticové klávesnice. Nuntá úprava SW
5. Řízení I2C. Deska má na konektor vyvedené piny SDA, SCL. Desku tak lze řídit pomocí I2C (nutná úprava SW), nebo lze zcela nahradit část věnovanou servo pomocí externího driveru pro serva.

## Stručný popis zapojení

Jádrem zapojení je efektivní využití výstupů Arduina. Jelikož má pouze 6 PWM výstupů, 4 PWM výstupy jsou vedené na demultiplexory 4052 (IC2 - přímo na základní desce, další na volitelné rozšiřující), čímž je možné ovládat 8 (16) servo, vždy až 2 (4) zároveň - zároveň je možné přestavovat ta serva, která používají výstupy demultiplexoru ve stejné skupině. Na desce jsou tyto výstupy uspořádané do skupin po dvou. Adresa pro demultiplexor a další 2 PWM výstupy společně s napájením jsou vyvedené na konektor pro přídavnou servo desku (+8 servo). 

Dále při zapnutí, či po resetu Arduino nejprve generuje řídící signál, a teprve poté sepne napájení pro danou skupinu servo. Napájení serv se nepřerušuje. Pro spínaní napájení se používají tranzistory MOSFET Q1-4, spíná se napájení (ne GND). Spínané napájení je vyvedeno na druhém konektoru pro pomocnou servo desku, takže pomocná deska neobsahuje žádnou "vyšší" logiku kromě demultiplexoru 4052. Napájení servo je regulovatelné, v rozsahu cca 3-7V, pro větší rozsah je nutné změnit velikos odporů R9 a R10.

Druhá část desky ovládá posuvný registr (IC1), který je použitý pro spínané výstupy. Prvních 8 výstupů je vyvedeno přímo na základní desce, pomocí tranzistorového pole (IC5) je možné spínat menší, i induktivní (motorky), zátěž. Dále je vyvedeno ovládání a napájení pro další posuvné konektory (další rozšiřující desky). Celkem je možné ovládat 64 výstupů - na tolik jsou dimenzovaná pole v obslužném SW. 

Na výstupní konektory "klávesnice" jsou vyvedené užitečné piny Arduina, které umožňují - po úpravě obslužného SW - nahradit maticový vstup jiným mechanismem. Samotný SW je psaný pomerně modulárně, takže je jednoduché vyměnit vstupní část za např. odečítání ze sběrnice RS485.

Rozsah kódu je bohužek takový, že neumožnil začlenit knihovnu k PWM řízení jasu na výstupech posuvného registru - obslužný SW zabírá téměř veškerou FLASH paměť. Pokud přispěvatelé zkrátí kód o cca 4kByte, bude možné doplnit ještě řízení úrovně jasu včetně plynulého zvyšování a snižování, či postupný náběh (zhasnutí).

## Příklady nastavení

Možný příklad nastavení polohy serv:
>    RNG:1:80:120:2      -- rozsah pro servo 1: 80 - 120 stupňů, rychlost pohybu 2
>    RNG:2:130:90:1      -- rozsah pro servo 2: 130 - 90 stupňů, rychlost pohybu 1
>    SFB:1:9             -- ohlas serva 1 na výstup #9
>    SFB:2:10            -- ohlas serva 2 na výstup #10

Poloha serva se dá nastavit interaktivně pomocí
>    CAL:1

Kdy se pak tlačítky nastaví poloha páky serva a potvrdí se. 

Příklad nastavení "vlakové cesty" pro kolejovou spojku, kdy přepínač přepíná obě výhybky kolejové spojky na "odbočku", či "rovně". Jelikož jde o serva 1 a 2, přestavují se najednou
>   DEF:1:2:B           -- nastavení povelu #1, tlačítko #2. "B" značí "reverz" po uvolnění tlačítka
>   MOV:1:L             -- servo 1 do levé polohy
>   MOV:2:R             -- servo 2 do pravé polohy
>   FIN                 -- ukončení sekvence

Totéž, ale s požadavkem na **postupné** přestavení serv, kdy se čeká na dokončení předchozího pohybu
> DEF:1:2:B:W           -- volitelné W na konci nařídí čekání

Nebo pro postupné přestavování s větší prodlevou:
>   DEF:1:2:B           -- nastavení povelu #1, tlačítko #2. "B" značí "reverz" po uvolnění tlačítka
>   WAI:10              -- prodleva v násobku 50ms, tedy 500ms
>   MOV:1:L             -- servo 1 do levé polohy
>   WAI:10              -- prodleva v násobku 50ms
>   MOV:2:R             -- servo 2 do pravé polohy
>   FIN                 -- ukončení sekvence

Dvojtlačítkové ovládání jeřábu (servo #3): jedno tlačítko (#4) hýbe vlevo, druhé vpravo (#5). Uvolnění tlačítka jeřáb zastaví
>   RNG:3:0:180:8      -- rozsah pro servo 3: 0-180 stupňů, co nejpomaleji
>   DEF:2:4:A           -- nastavení povelu #2, tlačítko #4. "A" značí přerušení povelu po uvolnění
>   MOV:3:L             -- natáčet servo 3 do levé polohy; zrušení příkazu (uvolnění tlačítka) stopne servo
>   FIN                 -- ukončení sekvence

>   DEF:3:5:A           -- nastavení povelu #3, tlačítko #5.
>   MOV:3:R             -- natáčet servo 3 do pravé polohy; zrušení příkazu (uvolnění tlačítka) stopne servo
>   FIN                 -- ukončení sekvence

Současné stisknutí obou tlačítek nijak ošetřeno není - jelikož se ale jedná o jediné servo, v pořadí druhý povel vyčká až se zcela vykoná prvni (servo dojede do jedné z krajních poloh). 

Rozsvícení domečku na určitou dobu, avšak opětovným stiskem téhož tlačítka domek zhasne ihned:
>   DEF:4:1:R           -- stisk tlačítka 1 sekvenci zapne, opětovný stisk "vrátí"
>   OUT:1:+             -- zapnout výstup 1
>   WAI:1000            -- čekat 50ms * 1000 tzn. 50 sekund
>   OUT:1:-             -- vypne výstup. Při "vracení" nemá vliv

Pozor - čekající příkazy se počítají do běžících sekvencí, jejichž počet je omezen (cca 20). Při naplnění kapacity se nedají provést ani "bežné" sekvence jako točení serva.

