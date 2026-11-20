# Simulador RISC-V (RV32I)

Simulador del ISA base RV32I hecho en C++ para la Tarea 5 de Arquitectura de Computadores (CS3051, 2026-II).

Carga un binario crudo en la dirección 0x0 y lo ejecuta paso a paso desde la terminal.

## Compilar

```
make
```

Necesita `g++` con soporte para C++17.

## Usar

```
./riscv-sim tests/riscvtest.bin
```

Comandos:

| Comando | Qué hace |
|---|---|
| `step [n]` | ejecuta n instrucciones (1 por defecto) |
| `run` | ejecuta hasta ebreak, exit, breakpoint, error o loop infinito |
| `pc` | muestra el PC |
| `regs [r ...]` | muestra registros (`x5`, `a0`, `sp`...); sin argumentos, todos |
| `mem ini [fin]` | muestra bytes de memoria |
| `disasm [addr] [n]` | desensambla desde una dirección |
| `break addr` / `delete addr` | agrega o quita un breakpoint |
| `reset` / `load archivo` | reinicia la CPU / carga otro programa |
| `exit` | salir |

Los números se pueden escribir en decimal o en hexadecimal (`0x1000`).

## Programas de prueba

Están en `tests/` (el `.s` y el `.bin` de cada uno):

- `riscvtest`: deja 25 (0x19) en la dirección 0x64.
- `quicksort`: ordena el arreglo en 0x1000.
- `symmetric_tree`: termina con `a0 = 1`.
- `fibonacci`: imprime los primeros 10 números usando `ecall`.

## Limitaciones

- Solo RV32I (sin extensión M).
- No carga archivos `.elf` ni tiene soporte para GDB.
- Los binarios de `tests/` fueron ensamblados con un script propio, no con el ensamblador de CPUlator.