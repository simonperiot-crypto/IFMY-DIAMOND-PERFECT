# ◆ IFMY DIAMOND PERFECT

Suite vocale tout-en-un pour **Logic Pro** (Mac) : autotune temps réel, et à terme harmonies, vocodeur, bender, édition graphique, dynamique, EQ et effets, avec un système de presets complet.

Plugin **Audio Unit** (+ VST3), universel Apple Silicon / Intel, macOS 12 ou plus récent.

---

## Télécharger et installer

1. Onglet **Actions** du dépôt → dernier build vert « Build IFMY Diamond Perfect » → en bas, **Artifacts** → `IFMY-Diamond-Perfect-mac`.
   *(Ou onglet **Releases** pour les versions taguées.)*
2. Dézippe, puis double-clique sur **Installer.command** (la première fois : clic droit → Ouvrir).
3. Relance Logic Pro.

| Version | Où la trouver dans Logic | À quoi elle sert |
|---|---|---|
| **IFMY Diamond Perfect** | Audio FX → Audio Units → IFMY | L'effet sur ta piste voix |
| **IFMY Diamond Perfect MIDI** | Piste instrument → slot Instrument → AU MIDI-controlled Effects → IFMY, puis menu *Side Chain* = ta piste voix | Piloter la justesse au clavier (puis harmonies et vocodeur) |

**Le plugin n'apparaît pas ?** Logic → Réglages → Gestionnaire de plug-ins → IFMY → *Réinitialiser et rescanner la sélection*.

---

## Phase 2 (v0.2.0) : TUNE avancé

- Moteur **STUDIO** (formants conservés) et moteur **LIVE** (~3 ms) pour enregistrer
- **Gorge** et **formants**, **Flex-Tune**, mode **Classic**, **vibrato** ajouté, **plage vocale**
- **Key Finder** : détecte la tonalité de ta voix ou de l'instru (side-chain), puis l'applique en 1 clic
- 19 presets d'usine

## Phase 1

- **TUNE** : autotune temps réel, 12 tonalités × 48 gammes, vitesse, intensité, humanize, transpose, référence La 430 à 450 Hz, notes forcées au clavier MIDI, courbe de justesse en direct
- **DYNAMICS** : gate, de-esser, compresseur, saturation
- **EQ** : coupe-bas, boue 300 Hz, présence, air, avec courbe de réponse
- **FX** : doubleur, delay synchronisé au tempo de Logic (ping-pong), reverb
- **MASTER** : gains, anti-saturation, bypass sans clic
- **Presets** : 14 presets d'usine, enregistrer, renommer, supprimer, importer, exporter, favoris, A/B, undo/redo
- Modes **SIMPLE / EXPERT**, fenêtre redimensionnable

## Feuille de route

| Phase | Contenu |
|---|---|
| 1 ✅ | Squelette, CI, TUNE, dynamique, EQ, FX, presets |
| 2 ✅ | TUNE avancé : détection robuste, moteur Live ~3 ms, formants, gorge, Flex-Tune, Classic, vibrato, Key Finder |
| 3 | Dynamics/EQ/FX complets (EQ 8 bandes, ducking, Unmask side-chain…) + **50 presets** |
| 4 | **HARMONY** : 4 voix, 15 styles, chœur 32 voix, MIDI |
| 5 | **VOCODER** + voix synthétiques + **BENDER** + mode EFX (motifs de pitch rythmiques) |
| 6 | **GRAPH** : édition note par note |
| 7 | Vocal Assistant, débruitage IA, resynthèse, finitions |

---

## Garanties anti-bug (vérifiées à chaque build)

- `auval` (le test officiel de Logic) sur les 2 versions
- `pluginval` **niveau 10** (stress test maximal) sur les 2 versions
- 53 tests DSP : justesse à 44,1 / 48 / 88,2 / 96 kHz, moteurs Studio et Live, formants, gorge, Flex-Tune, vibrato, anti-saut d'octave, niveau conservé, Key Finder, aucun NaN
- Thread audio sans allocation ni verrou, traitement par tranches (tous buffers), paramètres lissés, anti-dénormaux, anti-NaN
- IDs de paramètres figés (`Source/Params.h`) : les projets Logic et les presets restent compatibles d'une version à l'autre

## Pour les développeurs

```bash
cmake -B build -G Xcode
cmake --build build --config Release --target DiamondPerfect_AU DiamondPerfectMIDI_AU
```

Codes AU : fabricant `Ifmy`, effet `Idmd` (aufx), MIDI `Idmm` (aumf). **Ne jamais les changer.**
Chaque release : incrémenter `VERSION` dans `CMakeLists.txt` et ajouter une entrée au `CHANGELOG.md`.

Fonctions inspirées des grandes suites vocales du marché, mais code, design, noms et presets 100 % originaux.
