# True Hotkeys — Features (v1.1.0)

An SKSE plugin that turns your keyboard into a real hotkey system for
weapons, spells, shouts, outfits, and more. Configured entirely in-game.
No INI editing required (though profiles are plain, human-readable text
if you ever want to).

## Four binds per key

Every key on your keyboard can hold up to four completely independent
binds, using a tap vs. hold press and an optional modifier key
(configurable — Caps Lock, Left Shift, whatever you like):

- Tap
- Hold
- Modifier + Tap
- Modifier + Hold

That's four times the binds without hunting for unused keys.

## What you can bind

- **Weapon sets** — equip a right-hand and/or left-hand weapon (or a
  shield) in one press, dual-wielding included, with an optional ammo
  swap bundled in.
- **Outfits** — swap a full hand-picked gear set, or an entire outfit
  from your load order, in one press. Optionally strip everything else
  you're wearing first for a clean swap.
- **Spells** — equip a different spell in each hand, or the same spell
  in both for dual-casting.
- **Shouts** — equip your shout of choice instantly.
- **Consumables** — drink a potion or eat food on demand.
- **Ammo swaps** — switch arrow/bolt type without touching your weapon.
- **Torch toggle** — equip or unequip your torch.
- **First/third person toggle** — flip camera view with a tap.
- **Unequip/panic button** — strip weapons, spells, armor, shouts,
  and/or ammo (any combination), or unequip specific worn items one at
  a time.
- **Movement remapping** — rebind forward/back/strafe-left/strafe-right
  to whatever keys you actually want to use.

## Stack multiple actions on one key

A single bind isn't limited to one action. You can equip a weapon set, 
cast a buff spell, and swap your outfit all from a single keypress, in
whatever order you choose.

## Toggle behavior

Press a weapon, spell, shout, or torch bind again to instantly unequip
it (optional setting). No more digging through inventory to take off gear.

## Profiles

Save entire hotkey setups as named profiles and switch between them
in-game. Perfect for different characters, builds, or playstyles.
Each profile can have its own dedicated hotkey to cycle through your
saved profiles without ever opening a menu. Saved to INI profiles, 
allowing for persistent key binds across multiple characters.

## Plays nice with vanilla and other mods

Uses an entirely new input layer, allowing you to choose, per bind, 
whether it should override Skyrim's own hotkey behavior or other mods' 
menu keys. Or you can leave them working exactly as normal.

## Built for fast setup

- Filter and sort every item picker by name, weapon type, armor class,
  spell school, and more.
- One-click "Add Equipped" buttons fill a bind straight from whatever
  you're currently wearing/wielding.
- Search your whole load order, or narrow pickers down to just what
  you're already carrying.
- On-screen notification and sound cue (optional) confirm every
  trigger.

## Configurable to taste

- Adjustable hold-vs-tap timing.
- Confirmation prompts before overwriting or deleting binds/profiles
  (toggleable).
- Auto-save on every change, or save manually when you're ready.
- Option to let hotkeys work inside menus for players using mods like
  Skyrim Souls that keep the game running in menus.

