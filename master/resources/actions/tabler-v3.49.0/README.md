# Tabler Icons outline v3.49.0 — Teacher action-icon sources

These files are the provenance inputs for the NextSpace Teacher Console action icons (V3an, GC2-S1), the
Teacher view-strip, grid-control and About icons (V3ao, GC2-S2a), the Teacher Screenshots-panel Show and
Delete icons (V3ap, GC2-S2b), the Teacher Slideshow-panel Previous and Next icons (V3aq, GC2-S2c), the
Teacher Slideshow-panel start/pause icon pair (V3ar, GC2-S2d: play for the unchecked Off state, pause for the
checked On state) and the Teacher Spotlight-panel Add, Remove and realtime icons (V3at, GC2-S2f:
device-desktop-plus for Add, device-desktop-minus for Remove, refresh-off for the unchecked realtime Off state
and the existing refresh for the checked On state).

The Remote Access toolbar (V3aw, GC2-S2i) adds `keyboard` for Send shortcut and `x` for Exit, which closes only
the Remote Access window, and shares five reviewed glyphs: `eye` (View only, the unchecked Off state) and
`pointer` (Remote control, the checked On state) for the View-only toggle, `devices-pc` for Select screen,
`camera` for Screenshot, and `maximize` (Fullscreen, Off) and `app-window` (Window, On) for the Fullscreen toggle.

The Send Files list and the Collect Files tree (V3ax) add four transfer-state glyphs, each drawn for both Off and
On: `clock` for Scheduled (queued), `transfer` for Transferring (direction-neutral for send and collect),
`hourglass` for Waiting (Collect Files only) and `circle-check` for Finished. The file and collection names and
the numeric progress remain independent cues.

The Administration (Configurator) page selector (V3bb) adds six glyphs, each drawn for both Off and On: `settings`
for General, `presentation` for Teacher Console, `shield-lock` for Access control, `key` for Authentication keys,
`brand-windows` for Windows and `hierarchy-2` for LDAP/AD. It reuses five reviewed glyphs unchanged: `device-desktop`
for Student Agent, `devices-pc` for Locations & computers, `screen-share` for the Demo server page (role Broadcast),
`apps` for Applications & websites and `transfer` for File transfer. The page titles remain the primary cues.
`brand-windows` depicts the Microsoft Windows logo; it is approved for this bounded slice only and is pending the
deferred Microsoft trademark-use review. Tabler's MIT licence does not decide third-party trademark rights.

The Administration (Configurator) menus (V3bc) reuse four reviewed glyphs unchanged for the four `V3AM-090`
commands, each drawn for both Off and On in its owning menu's palette: `device-floppy` for Save settings to file,
`folder-open` for Load settings from file and `trash` for Reset configuration in the File menu, and `info-circle` for
About in the Help menu. No source or generated glyph changed; the action labels, shortcuts and the existing reset
confirmation remain the primary cues.

The Administration (Configurator) Access Control page (V3bd) adds five glyphs for its nine `V3AM-091` command
buttons, each drawn for both Off and On in the receiving button's palette: `arrow-right` to authorize the selected
group, `arrow-left` to remove the selected authorized group, `plus` to add a rule, and `arrow-up` and `arrow-down` to
move the selected rule. It reuses three reviewed glyphs unchanged: `circle-check` for both Test buttons, `trash` to
remove a rule and `pencil` to edit the selected rule. The button labels and tooltips remain the primary cues.

The Administration (Configurator) General page (V3be) reuses three reviewed glyphs unchanged for its three `V3AM-093`
command buttons, each drawn for both Off and On in the receiving button's palette: `circle-check` to test the
authentication setup, `folder-open` for the log file directory button and `trash` to clear all log files, which keeps
its existing Student Agent stop confirmation. No source or generated glyph changed; the button labels and the directory
button's tooltip remain the primary cues.

The Administration (Configurator) Teacher Console page (V3bf) reuses three reviewed glyphs unchanged for its five
icon-only `V3AM-094` command buttons, each drawn for both Off and On in the receiving button's palette: `folder-open`
for the user configuration, screenshot and configuration templates directory buttons, `arrow-right` to disable the
features selected in All features and `arrow-left` to enable the features selected in Disabled features. No source or
generated glyph changed; each button's tooltip and accessible name remain the primary cues, and the feature list icons
are unchanged.

The Administration (Configurator) Student Agent page (V3bh) adds `player-stop` for the `V3AM-096` Stop service button
and reuses the reviewed `player-play` unchanged for its Start service button; each is drawn for both Off and On in the
receiving button's palette. The visible button labels and the service state text remain the primary cues.

The service-control Toast (V3bi) adds `alert-triangle` and `circle-x` and reuses `circle-check`, `info-circle` and `x`
unchanged for the five `V3AM-097` Toast icon masks: `circle-check` for Success, `alert-triangle` for Warning,
`circle-x` for Error, `info-circle` for Information and `x` for the close button. The action-icon renderer does not
draw them: `tools/v3bi_toast_icons.py` in the outer NextSpace project rasterises the five pinned sources
deterministically (2.1 view-box-unit stroke, round caps and joins, 8x supersampling, exact area downscaling) into the
existing white, alpha-only masks `core/resources/toast-success.png`, `toast-warning.png`, `toast-error.png` and
`toast-information.png` (196 px) and `toast-close.png` (134 px), which Toast recolours at run time. The generated
path data of the two new sources exists only because every approved source is generated; no renderer role maps to
them. Each toast's title and text remain the primary cues, and the icon-only close button is named
`Close notification`.

The Administration (Configurator) Authentication Keys page (V3bj) reuses the reviewed `folder-open` unchanged for its
two `V3AM-099` directory buttons, `openPublicKeyBaseDir` and `openPrivateKeyBaseDir`, through two separate roles (the
public and the private key directory); each is drawn for both Off and On in the receiving button's palette. No source
or generated glyph changed; the icon-only buttons are named `Open public key directory` and
`Open private key directory`.

The Administration (Configurator) Locations & computers page (V3bk) reuses the reviewed `plus`, `trash`, `arrow-up`
and `arrow-down` unchanged for its eight `V3AM-101` buttons through eight separate roles: `addLocationButton` and
`addComputerButton` draw `plus`, `removeLocationButton` and `removeComputerButton` draw `trash`,
`moveLocationUpButton` and `moveComputerUpButton` draw `arrow-up`, and `moveLocationDownButton` and
`moveComputerDownButton` draw `arrow-down`. Location and computer roles stay distinct; each is drawn for both Off and
On in the receiving button's palette. No source or generated glyph changed; each icon-only button's accessible name is
its existing translated tooltip.

The Administration (Configurator) Applications & websites page (V3bl), the Desktop Services plugin page, reuses the
reviewed `plus` and `trash` unchanged for its four `V3AM-104` buttons through four separate roles:
`addApplicationButton` and `addWebsiteButton` draw `plus`, and `removeApplicationButton` and `removeWebsiteButton`
draw `trash`. Application and website roles stay distinct; each is drawn for both Off and On in the receiving button's
palette. No source or generated glyph changed; each icon-only button's accessible name is its existing translated
tooltip.

The Administration (Configurator) File transfer page (V3bm) reuses the reviewed `folder-open` unchanged for its four
`V3AM-106` directory buttons through four separate roles: `browseDefaultSourceDirectory`,
`browseDestinationDirectory`, `browseFilesToCollectSourceDirectory` and `browseCollectedFilesDestinationDirectory`.
The four directory roles stay distinct; each is drawn for both Off and On in the receiving button's palette. No source
or generated glyph changed; each icon-only button gets an exact translated tooltip, which is also its accessible name.

- Family: Tabler Icons outline, tag `v3.49.0`, commit `bbed884d15354b5cebf2493371f20dc2d5e83eaf`.
- Source: https://github.com/tabler/tabler-icons/tree/bbed884d15354b5cebf2493371f20dc2d5e83eaf
- Each `*.svg` here is byte-identical to `icons/outline/<name>.svg` at that commit, fetched from
  `https://raw.githubusercontent.com/tabler/tabler-icons/bbed884d15354b5cebf2493371f20dc2d5e83eaf/icons/outline/<name>.svg`.
- `LICENSE` is byte-identical to the tagged upstream MIT licence. The notice shipped with the product is
  `THIRD-PARTY-NOTICES.txt` at the repository root.
- `manifest.tsv` records the SHA-256, strokes, consumers, treatment and modification of every source glyph.

The SVG files are not loaded at runtime and are not listed in any Qt resource file. The product compiles the
committed, generated `master/src/TeacherActionIconData.h`; normal builds do not run Python, Qt SVG, a font or a
web request. Only the 58 glyphs listed in `manifest.tsv` are approved; do not add or replace a glyph without a
reviewed specification.

## Regenerate and verify

From the `veyon` source directory, with Python 3 (standard library only):

```
py master/resources/actions/tabler-v3.49.0/generate_tabler_paths.py
py master/resources/actions/tabler-v3.49.0/generate_tabler_paths.py --check
```

The first command rewrites `master/src/TeacherActionIconData.h`; the second regenerates it in memory and exits
non-zero unless the committed header matches byte for byte. Both verify every source against its manifest
SHA-256 and reject any SVG construct outside the subset the 58 sources use.
