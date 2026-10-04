# Low Link Scope

Osciloskop pro ladění fáze mezi kopákem a basou. Plugin dáš na obě stopy a instance se propojí samy, bez sidechainu. V jednom okně pak uvidíš obě vlny přes sebe, zarovnané na beat.

Formáty: **VST3** (Cubase, Ableton, Reaper, Studio One, FL Studio…) a **CLAP** (Bitwig, Reaper). Zatím jen Windows.

## Build

**Varianta A, GitHub (nic neinstaluješ):** nahraj složku do nového repozitáře na GitHubu. Workflow v `.github/workflows/build.yml` plugin sám postaví. Hotový build stáhneš v záložce *Actions → poslední běh → Artifacts → LowLinkScope-Windows*.

**Varianta B, lokálně:** nainstaluj Visual Studio 2022 s workloadem *Desktop development with C++* a Git. Pak spusť `build-windows.bat`. JUCE se stáhne automaticky.

**Instalace:** zkopíruj celou složku `Low Link Scope.vst3` do `C:\Program Files\Common Files\VST3` a v Cubase dej *Studio → VST Plug-in Manager → Rescan*.

## Použití

1. Vlož plugin na stopu kopáku i na stopu basy (jako insert, ideálně jako poslední v řetězci).
2. Název stopy se převezme z DAW automaticky. Pokud ne, napiš ho do pole **THIS TRACK**.
3. Na kopáku vyber v **COMPARE WITH** basu.
4. Spusť přehrávání. Zobrazuje se vždy poslední celé okno (např. 1 beat), takže obraz stojí.

| Ovládání | Co dělá |
|---|---|
| SIZE | Délka okna: 1/4 až 4 beaty, zarovnané na grid písně |
| LOW-PASS | Filtr jen pro zobrazení a analýzu (bez fázového posunu). Na kick/bass se hodí 80–200 Hz |
| This / Both / Linked | Která vlna se zobrazí |
| Sum | Přidá bílou vlnu součtu, tedy co z obou reálně zbyde |
| AMP, THIS, LINKED | Výška zobrazení a intenzita každé vlny |
| ZOOM, POSITION | Přiblížení a posun v okně |
| Freeze | Zmrazí aktuální obraz, abys mohl porovnávat proti změnám |

Plugin zvětšíš tahem za pravý dolní roh.

**Červené pruhy** ve vlně označují místa, kde mají obě stopy opačnou polaritu a navzájem se vyrušují.

**Spodní lišta:**
- **SUM vs SEPARATE:** kolik dB získáš nebo ztratíš sečtením oproti samostatným stopám. Záporné číslo znamená, že se basové pásmo vyrušuje.
- **PHASE:** korelace −1 až +1. Zelená je v pořádku, červená znamená protifázi.
- **SUGGESTION:** o kolik ms posunout druhou stopu, případně otočit polaritu, a kolik dB to přinese. V Cubase použij *Track Delay* v Inspektoru (ms, kladné = později) a pro polaritu *Phase* tlačítko v kanálu.

## Dobré vědět

- Audio plugin nijak nemění, jen ho čte. Má nulovou latenci.
- Přesné zarovnání funguje při přehrávání. Když transport stojí, zobrazení je jen přibližné (upozorní na to).
- Pokud se zobrazí „Linked track not processing", Cubase uspal plugin na tiché stopě. Vypni *Studio → Studio Setup → VST Plug-ins → Suspend VST3 plug-in processing when no audio signals are received*.
- Najednou může běžet až 16 instancí. Všechny musí běžet na stejné vzorkovací frekvenci.

## Struktura

- `Source/LinkBus.*`: sdílená paměť mezi instancemi (funguje i mezi procesy)
- `Source/Analysis.*`: low-pass bez fázového posunu, ztráta v dB, korelace, hledání posunu a polarity
- `Source/PluginProcessor.*`: zápis audia podle pozice na časové ose
- `Source/PluginEditor.*`, `Source/ScopeView.*`: GUI
- `Tests/AnalysisTests.cpp`: testy analýzy a linku (`-DLOWLINK_BUILD_TESTS=ON`)
