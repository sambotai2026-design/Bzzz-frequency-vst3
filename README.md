# B'Zzz Frequency — VST3 (V2 FULL WORKSTATION)

Plugin instrument techno **Paye Ta Ruche** : la version web complète portée en natif.
Drums 12 pistes 100% synthèse + **FABLE ENGINE** (synthé) + les **2000 presets**
identiques à la version web (même générateur seedé).

## Contenu V2
- **12 pistes drums** synthétisées (kick, rumble, snare, clap, hats 808 métalliques, ride, perc, tom, acid, stab, rise)
- **Tranche par piste** : filtre (LP/HP/BP/NOTCH/PEAK), drive, crush, decay, pitch, pan, sends delay/reverb, volume
- **FABLE ENGINE** : 2 OSC + sub + noise, unison 7 voix, FM, glide 303, 6 modes de filtre
  (LP24/LP12/HP/BP/NOTCH/FORMANT), ADSR, LFO (filtre + pitch), drive, crush, largeur stéréo
- **Séquenceur synthé 16 pas** avec notes, accents et **slides 303**, octave ±
- **1000 kits + 1000 patches** : les mêmes presets que le plugin web (port exact du générateur)
- **Patterns A–H** (drums + séquence synthé) avec copie
- **MODE SONG** : enchaînement de patterns avec répétitions ×1/×2/×4/×8
- **FX** : ping-pong delay synchronisé au tempo, reverb, **sidechain** (le kick duck le synthé et les FX)
- **Master** : filtre, drive, glue comp, limiter
- **6 styles** (Berlin, Detroit, London, Paris, Rotterdam, Tbilisi) : patterns + mélodies signatures
- Synchronisation **parfaite** au tempo et transport d'Ableton (swing réglable)
- **MIDI** : canal 10 = drums (GM), autres canaux = synthé polyphonique
- Tout l'état (patterns, song, kits, réglages) **sauvegardé dans le projet Live**
- Macros automatisables : master (cutoff, réso, drive, volume, delay, reverb, sidechain, swing)
  et synthé (cutoff, réso, env, LFO rate/amt, volume)

## Compiler avec GitHub Actions (recommandé — aucun outil à installer)
1. Crée un dépôt GitHub public et uploade **tout le contenu** de ce dossier.
2. Le fichier `.github/workflows/build.yml` déclenche la compilation automatiquement.
3. Onglet **Actions** → attends le ✔ vert (~10 min) → clique le run → section **Artifacts** :
   - `BZzz-Frequency-VST3-macOS` (universel Intel + Apple Silicon)
   - `BZzz-Frequency-VST3-Windows`

## Installer
**macOS** : copie `B'Zzz Frequency.vst3` dans `~/Library/Audio/Plug-Ins/VST3/` puis :
```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/B\'Zzz\ Frequency.vst3
```
**Windows** : copie le dossier dans `C:\Program Files\Common Files\VST3\`

Puis dans Ableton : Préférences → Plug-ins → VST3 activé → Rescan.

## Compiler localement (optionnel)
```
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git
cmake -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR="$PWD/JUCE"
cmake --build build --config Release
```

## Notes
- Le tempo appartient à l'hôte : le plugin suit Ableton, les styles ne changent jamais le BPM.
- Le réacteur visuel du plugin web n'est pas porté (interface native dédiée à la production).
