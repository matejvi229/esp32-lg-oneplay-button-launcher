# Přepis IR kódů - kopie konverzace

Toto je kopie konverzace mezi ChatGPT a anonymním uživatelem.

Nahlásit konverzaci

Řekl/a jsi:
Nahraný obrázek
Přepis mi tyhle kódy infra ovladače do přehledného souhrnu

ChatGPT řekl:
Z přiloženého snímku jsem přepsal všechny ručně zapsané IR kódy do strukturovaného a konzistentního souhrnu. Zachovávám původní hexadecimální notaci (`0xXX`).

## SET TOP BOX - IR kódy tlačítek

### Napájení

| Tlačítko | Kód |
|---|---|
| OFF | `0x0C` |

### Barevná tlačítka

| Tlačítko | Kód |
|---|---|
| ČERVENÉ (ČER) | `0x6B` |
| ZELENÉ (ZEL) | `0x6C` |
| ŽLUTÉ (ŽLV) | `0x6D` |
| MODRÉ (MOD) | `0x6E` |

### Ovládání přehrávání

| Funkce | Kód |
|---|---|
| Přetáčení zpět (⏪) | `0x51` |
| Stop (□) | `0x36` |
| Přetáčení vpřed (⏩) | `0x50` |
| REC | `0x3F` |

### Menu a navigace

| Funkce | Kód |
|---|---|
| MENU | `0x12` |
| EPG | `0x41` |
| TXT | `0x3C` |
| OK (střed) | `0x16` |
| Nahoru (↑) | `0x10` |
| Dolů (↓) | `0x11` |
| Vlevo (←) | `0x15` |
| Vpravo (→) | `0x16` (pravděpodobně kolize s OK - viz poznámka níže) |
| Zpět | `0x1F` |
| Info (I) | `0x4A` |
| TV | `0x1D` |

### Hlasitost a kanály

| Funkce | Kód |
|---|---|
| Hlasitost + | `0x0E` |
| Hlasitost - | `0x0A` |
| Kanál + | `0x14` |
| Kanál - | `0x17` |
| MUTE | `0x0D` |

### Numerická klávesnice

| Tlačítko | Kód |
|---|---|
| 0 | `0x00` |
| 1-9 | `0x01 - 0x09` |

### Další tlačítka

| Funkce | Kód |
|---|---|
| NIC | neuvedeno |
| MK | `0x27` |
| X | `0x25` |
| Q | `0x26` |
| `0x61` | neoznačená funkce |

## Poznámky

- Kolize kódu `0x16` - na nákresu je tento kód přiřazen jak tlačítku OK, tak pravé šipce. Doporučuji ověřit sniffováním IR signálu (např. pomocí TSOP + Arduino / logic analyzer).
- Některé položky (`NIC`, `0x61`) nejsou jednoznačně popsány.
- TXT je označeno jako `0x3C`, ale je částečně přeškrtlé - možná bylo opravováno.

Pokud chceš, mohu:

- převést to do C struktury / enumu
- vytvořit Python dictionary
- připravit to jako Home Assistant configuration
- nebo udělat validaci rozsahu (např. zda jde o NEC protokol podle struktury kódů)
