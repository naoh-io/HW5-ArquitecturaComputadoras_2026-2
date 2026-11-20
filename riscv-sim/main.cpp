// Simulador RISC-V RV32I - CS3051 Arquitectura de Computadores
// Compilar: g++ -std=c++17 -O2 -Wall -o riscv-sim main.cpp
// Uso:      ./riscv-sim programa.bin

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using u8 = uint8_t;
using u32 = uint32_t;
using i32 = int32_t;

// ---------------------------------------------------------------------------
// Memoria plana byte-addressable de 32 bits, little-endian (paginas bajo demanda)
// ---------------------------------------------------------------------------
class Memory {
    static constexpr u32 PAGE_BITS = 12;
    static constexpr u32 PAGE_SIZE = 1u << PAGE_BITS;
    std::unordered_map<u32, std::array<u8, PAGE_SIZE>> pages;

public:
    u8 read8(u32 addr) const {
        auto it = pages.find(addr >> PAGE_BITS);
        return it == pages.end() ? 0 : it->second[addr & (PAGE_SIZE - 1)];
    }
    void write8(u32 addr, u8 v) {
        auto it = pages.find(addr >> PAGE_BITS);
        if (it == pages.end()) {
            it = pages.emplace(addr >> PAGE_BITS, std::array<u8, PAGE_SIZE>{}).first;
        }
        it->second[addr & (PAGE_SIZE - 1)] = v;
    }
    u32 read16(u32 a) const { return read8(a) | (read8(a + 1) << 8); }
    u32 read32(u32 a) const { return read16(a) | (read16(a + 2) << 16); }
    void write16(u32 a, u32 v) { write8(a, v & 0xff); write8(a + 1, (v >> 8) & 0xff); }
    void write32(u32 a, u32 v) { write16(a, v & 0xffff); write16(a + 2, v >> 16); }
    void clear() { pages.clear(); }
};

// ---------------------------------------------------------------------------
// Utilidades de decodificacion
// ---------------------------------------------------------------------------
static const char* REG_NAMES[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0",
    "a1",   "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5",
    "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

static inline u32 opcode(u32 i) { return i & 0x7f; }
static inline u32 rd_(u32 i) { return (i >> 7) & 0x1f; }
static inline u32 f3_(u32 i) { return (i >> 12) & 0x7; }
static inline u32 rs1_(u32 i) { return (i >> 15) & 0x1f; }
static inline u32 rs2_(u32 i) { return (i >> 20) & 0x1f; }
static inline u32 f7_(u32 i) { return i >> 25; }

static inline i32 imm_i(u32 i) { return (i32)i >> 20; }
static inline i32 imm_s(u32 i) { return ((i32)i >> 25) * 32 | (i32)((i >> 7) & 0x1f); }
static inline i32 imm_b(u32 i) {
    u32 v = (((u32)((i32)i >> 31)) << 12) | (((i >> 7) & 1) << 11) |
            (((i >> 25) & 0x3f) << 5) | (((i >> 8) & 0xf) << 1);
    return (i32)v;
}
static inline u32 imm_u(u32 i) { return i & 0xfffff000; }
static inline i32 imm_j(u32 i) {
    u32 v = (((u32)((i32)i >> 31)) << 20) | (i & 0xff000) |
            (((i >> 20) & 1) << 11) | (((i >> 21) & 0x3ff) << 1);
    return (i32)v;
}

// ---------------------------------------------------------------------------
// Desensamblador
// ---------------------------------------------------------------------------
static std::string hex(u32 v) {
    char b[16];
    snprintf(b, sizeof b, "0x%x", v);
    return b;
}

static std::string disasm(u32 inst, u32 pc) {
    std::ostringstream o;
    auto R = [](u32 r) { return std::string(REG_NAMES[r]); };
    u32 rd = rd_(inst), rs1 = rs1_(inst), rs2 = rs2_(inst), f3 = f3_(inst), f7 = f7_(inst);
    switch (opcode(inst)) {
    case 0x37: o << "lui " << R(rd) << ", " << hex(imm_u(inst) >> 12); break;
    case 0x17: o << "auipc " << R(rd) << ", " << hex(imm_u(inst) >> 12); break;
    case 0x6f: o << "jal " << R(rd) << ", " << hex(pc + imm_j(inst)); break;
    case 0x67: o << "jalr " << R(rd) << ", " << imm_i(inst) << "(" << R(rs1) << ")"; break;
    case 0x63: {
        static const char* n[8] = {"beq", "bne", "?", "?", "blt", "bge", "bltu", "bgeu"};
        o << n[f3] << " " << R(rs1) << ", " << R(rs2) << ", " << hex(pc + imm_b(inst));
        break;
    }
    case 0x03: {
        static const char* n[8] = {"lb", "lh", "lw", "?", "lbu", "lhu", "?", "?"};
        o << n[f3] << " " << R(rd) << ", " << imm_i(inst) << "(" << R(rs1) << ")";
        break;
    }
    case 0x23: {
        static const char* n[8] = {"sb", "sh", "sw", "?", "?", "?", "?", "?"};
        o << n[f3] << " " << R(rs2) << ", " << imm_s(inst) << "(" << R(rs1) << ")";
        break;
    }
    case 0x13: {
        static const char* n[8] = {"addi", "slli", "slti", "sltiu", "xori", "srli", "ori", "andi"};
        std::string name = n[f3];
        if (f3 == 5 && (f7 & 0x20)) name = "srai";
        if (f3 == 1 || f3 == 5) o << name << " " << R(rd) << ", " << R(rs1) << ", " << rs2;
        else o << name << " " << R(rd) << ", " << R(rs1) << ", " << imm_i(inst);
        break;
    }
    case 0x33: {
        static const char* n[8] = {"add", "sll", "slt", "sltu", "xor", "srl", "or", "and"};
        std::string name = n[f3];
        if (f3 == 0 && (f7 & 0x20)) name = "sub";
        if (f3 == 5 && (f7 & 0x20)) name = "sra";
        o << name << " " << R(rd) << ", " << R(rs1) << ", " << R(rs2);
        break;
    }
    case 0x0f: o << "fence"; break;
    case 0x73: o << (imm_i(inst) == 0 ? "ecall" : imm_i(inst) == 1 ? "ebreak" : "system?"); break;
    default: o << ".word " << hex(inst); break;
    }
    return o.str();
}

// ---------------------------------------------------------------------------
// CPU
// ---------------------------------------------------------------------------
enum class Status { Ok, Exited, Break, Error };

struct Cpu {
    u32 pc = 0;
    u32 x[32] = {};
    Memory mem;
    int exit_code = 0;
    std::string error;

    void reset() { pc = 0; for (auto& r : x) r = 0; exit_code = 0; error.clear(); }

    void set(u32 r, u32 v) { if (r != 0) x[r] = v; }

    Status fail(const std::string& msg) { error = msg; return Status::Error; }

    // ecall estilo SPIM/CPUlator (numero de servicio en a7)
    Status ecall() {
        switch (x[17]) {
        case 1: printf("%d", (i32)x[10]); break;                  // print int
        case 4: {                                                 // print string
            for (u32 a = x[10]; mem.read8(a); a++) putchar(mem.read8(a));
            break;
        }
        case 5: {                                                 // read int
            long long v = 0;
            if (!(std::cin >> v)) v = 0;
            x[10] = (u32)v;
            break;
        }
        case 11: putchar((char)x[10]); break;                     // print char
        case 12: {                                                // read char
            char c = 0; std::cin.get(c); x[10] = (u8)c; break;
        }
        case 34: printf("0x%08x", x[10]); break;                  // print hex
        case 36: printf("%u", x[10]); break;                      // print unsigned
        case 10: exit_code = 0; fflush(stdout); return Status::Exited;
        case 17: case 93: exit_code = (int)x[10]; fflush(stdout); return Status::Exited;
        default: return fail("ecall no soportada: a7 = " + std::to_string(x[17]));
        }
        fflush(stdout);
        return Status::Ok;
    }

    Status step() {
        u32 inst = mem.read32(pc);
        u32 next = pc + 4;
        u32 rd = rd_(inst), rs1 = rs1_(inst), rs2 = rs2_(inst), f3 = f3_(inst), f7 = f7_(inst);
        u32 a = x[rs1], b = x[rs2];

        switch (opcode(inst)) {
        case 0x37: set(rd, imm_u(inst)); break;                       // lui
        case 0x17: set(rd, pc + imm_u(inst)); break;                  // auipc
        case 0x6f: set(rd, next); next = pc + imm_j(inst); break;     // jal
        case 0x67: {                                                  // jalr
            u32 t = (a + imm_i(inst)) & ~1u;
            set(rd, next);
            next = t;
            break;
        }
        case 0x63: {                                                  // branches
            bool take;
            switch (f3) {
            case 0: take = a == b; break;
            case 1: take = a != b; break;
            case 4: take = (i32)a < (i32)b; break;
            case 5: take = (i32)a >= (i32)b; break;
            case 6: take = a < b; break;
            case 7: take = a >= b; break;
            default: return fail("branch invalido en " + hex(pc));
            }
            if (take) next = pc + imm_b(inst);
            break;
        }
        case 0x03: {                                                  // loads
            u32 addr = a + imm_i(inst);
            switch (f3) {
            case 0: set(rd, (u32)(i32)(int8_t)mem.read8(addr)); break;
            case 1: set(rd, (u32)(i32)(int16_t)mem.read16(addr)); break;
            case 2: set(rd, mem.read32(addr)); break;
            case 4: set(rd, mem.read8(addr)); break;
            case 5: set(rd, mem.read16(addr)); break;
            default: return fail("load invalido en " + hex(pc));
            }
            break;
        }
        case 0x23: {                                                  // stores
            u32 addr = a + imm_s(inst);
            switch (f3) {
            case 0: mem.write8(addr, b & 0xff); break;
            case 1: mem.write16(addr, b & 0xffff); break;
            case 2: mem.write32(addr, b); break;
            default: return fail("store invalido en " + hex(pc));
            }
            break;
        }
        case 0x13: {                                                  // op-imm
            i32 imm = imm_i(inst);
            u32 sh = rs2;  // shamt = bits [24:20]
            switch (f3) {
            case 0: set(rd, a + imm); break;
            case 1: set(rd, a << sh); break;
            case 2: set(rd, (i32)a < imm); break;
            case 3: set(rd, a < (u32)imm); break;
            case 4: set(rd, a ^ imm); break;
            case 5: set(rd, (f7 & 0x20) ? (u32)((i32)a >> sh) : a >> sh); break;
            case 6: set(rd, a | imm); break;
            case 7: set(rd, a & imm); break;
            }
            break;
        }
        case 0x33: {                                                  // op
            if (f7 != 0x00 && f7 != 0x20)
                return fail("instruccion no soportada (extension M?) en " + hex(pc) + ": " + hex(inst));
            u32 sh = b & 0x1f;
            switch (f3) {
            case 0: set(rd, (f7 & 0x20) ? a - b : a + b); break;
            case 1: set(rd, a << sh); break;
            case 2: set(rd, (i32)a < (i32)b); break;
            case 3: set(rd, a < b); break;
            case 4: set(rd, a ^ b); break;
            case 5: set(rd, (f7 & 0x20) ? (u32)((i32)a >> sh) : a >> sh); break;
            case 6: set(rd, a | b); break;
            case 7: set(rd, a & b); break;
            }
            break;
        }
        case 0x0f: break;                                             // fence = nop
        case 0x73: {
            if (imm_i(inst) == 0) {
                Status s = ecall();
                if (s != Status::Ok) return s;
            } else if (imm_i(inst) == 1) {
                pc = next;
                return Status::Break;                                 // ebreak
            } else {
                return fail("instruccion SYSTEM no soportada en " + hex(pc));
            }
            break;
        }
        default:
            return fail("instruccion invalida " + hex(inst) + " en pc = " + hex(pc));
        }
        pc = next;
        return Status::Ok;
    }
};

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------
static bool parse_u32(const std::string& s, u32& out) {
    char* end;
    errno = 0;
    unsigned long long v = strtoull(s.c_str(), &end, 0);
    if (s.empty() || *end != '\0' || v > 0xffffffffull) return false;
    out = (u32)v;
    return true;
}

static int parse_reg(std::string s) {
    if (s.size() >= 2 && s[0] == 'x') {
        char* end;
        long n = strtol(s.c_str() + 1, &end, 10);
        if (*end == '\0' && n >= 0 && n < 32) return (int)n;
    }
    if (s == "fp") return 8;
    for (int i = 0; i < 32; i++)
        if (s == REG_NAMES[i]) return i;
    return -1;
}

static void print_reg(const Cpu& c, int r) {
    printf("x%-2d (%-4s) = 0x%08x  (%d)\n", r, REG_NAMES[r], c.x[r], (i32)c.x[r]);
}

static bool load_program(Cpu& cpu, const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    cpu.mem.clear();
    for (size_t i = 0; i < data.size(); i++) cpu.mem.write8((u32)i, (u8)data[i]);
    cpu.reset();
    printf("\"%s\" cargado a memoria (%zu bytes).\n", path.c_str(), data.size());
    return true;
}

static void print_help() {
    puts("Comandos:\n"
         "  step [n]            ejecuta n instrucciones (por defecto 1)\n"
         "  run                 ejecuta hasta ebreak, exit, breakpoint, error o loop infinito\n"
         "  pc                  muestra el PC\n"
         "  regs [r ...]        muestra registros (x5, a0, sp, ...); sin args muestra todos\n"
         "  mem ini [fin]       muestra memoria en bytes (hex), rango inclusivo\n"
         "  disasm [addr] [n]   desensambla n instrucciones desde addr (por defecto pc, 8)\n"
         "  break addr          agrega breakpoint; 'break' lista; 'delete addr' lo quita\n"
         "  reset               reinicia registros y PC (mantiene el programa)\n"
         "  load archivo.bin    carga otro programa\n"
         "  help                muestra esta ayuda\n"
         "  exit                sale");
}

// Imprime el resultado de ejecutar; devuelve true si la ejecucion debe detenerse.
static bool report(Cpu& cpu, Status s) {
    switch (s) {
    case Status::Ok: return false;
    case Status::Exited: printf("\nPrograma terminado con codigo %d.\n", cpu.exit_code); return true;
    case Status::Break: printf("ebreak alcanzado. pc = 0x%08x\n", cpu.pc); return true;
    case Status::Error: printf("Error de ejecucion: %s\n", cpu.error.c_str()); return true;
    }
    return true;
}

int main(int argc, char** argv) {
    Cpu cpu;
    std::set<u32> breakpoints;

    if (argc >= 2) {
        if (!load_program(cpu, argv[1])) {
            fprintf(stderr, "No se pudo abrir \"%s\".\n", argv[1]);
            return 1;
        }
    } else {
        puts("Uso: riscv-sim programa.bin  (o usa 'load archivo.bin')");
    }

    std::string line;
    while (true) {
        printf("> ");
        fflush(stdout);
        if (!std::getline(std::cin, line)) break;
        std::istringstream in(line);
        std::vector<std::string> t;
        for (std::string w; in >> w;) t.push_back(w);
        if (t.empty()) continue;
        const std::string& cmd = t[0];

        if (cmd == "exit" || cmd == "quit" || cmd == "q") {
            puts("See you next time...");
            printf("Program exited with code %d.\n", cpu.exit_code);
            return 0;
        } else if (cmd == "help") {
            print_help();
        } else if (cmd == "pc") {
            printf("pc = 0x%08x\n", cpu.pc);
        } else if (cmd == "step" || cmd == "s") {
            u32 n = 1;
            if (t.size() > 1 && !parse_u32(t[1], n)) { puts("Numero invalido."); continue; }
            for (u32 i = 0; i < n; i++) {
                u32 pc = cpu.pc;
                std::string d = disasm(cpu.mem.read32(pc), pc);
                Status s = cpu.step();
                if (s == Status::Ok || s == Status::Break || s == Status::Exited)
                    printf("Ejecutado 0x%08x: %s\n", pc, d.c_str());
                if (report(cpu, s)) break;
            }
        } else if (cmd == "run" || cmd == "r") {
            const unsigned long long LIMIT = 100000000ull;
            unsigned long long count = 0;
            while (true) {
                u32 prev = cpu.pc;
                Status s = cpu.step();
                count++;
                if (report(cpu, s)) break;
                if (cpu.pc == prev) {  // la instruccion salta a si misma: fin tipico de programa
                    printf("Loop infinito detectado en 0x%08x (%llu instrucciones). Fin.\n", cpu.pc, count);
                    break;
                }
                if (breakpoints.count(cpu.pc)) { printf("Breakpoint en 0x%08x.\n", cpu.pc); break; }
                if (count >= LIMIT) { puts("Limite de instrucciones alcanzado."); break; }
            }
        } else if (cmd == "regs") {
            if (t.size() == 1) {
                for (int r = 0; r < 32; r++) print_reg(cpu, r);
            } else {
                for (size_t i = 1; i < t.size(); i++) {
                    int r = parse_reg(t[i]);
                    if (r < 0) printf("Registro invalido: %s\n", t[i].c_str());
                    else print_reg(cpu, r);
                }
            }
        } else if (cmd == "mem") {
            u32 a, b;
            if (t.size() < 2 || !parse_u32(t[1], a)) { puts("Uso: mem ini [fin]"); continue; }
            b = a;
            if (t.size() > 2 && !parse_u32(t[2], b)) { puts("Direccion invalida."); continue; }
            if (b < a) { puts("El fin debe ser >= inicio."); continue; }
            printf("Memoria (0x%x-0x%x):", a, b);
            for (unsigned long long addr = a; addr <= b; addr++) printf(" %02X", cpu.mem.read8((u32)addr));
            putchar('\n');
        } else if (cmd == "disasm" || cmd == "d") {
            u32 a = cpu.pc, n = 8;
            if (t.size() > 1 && !parse_u32(t[1], a)) { puts("Direccion invalida."); continue; }
            if (t.size() > 2 && !parse_u32(t[2], n)) { puts("Numero invalido."); continue; }
            for (u32 i = 0; i < n; i++, a += 4) {
                u32 inst = cpu.mem.read32(a);
                printf("%s0x%08x: %08x  %s\n", a == cpu.pc ? "=> " : "   ", a, inst, disasm(inst, a).c_str());
            }
        } else if (cmd == "break" || cmd == "b") {
            u32 a;
            if (t.size() == 1) {
                if (breakpoints.empty()) puts("Sin breakpoints.");
                for (u32 b : breakpoints) printf("  0x%08x\n", b);
            } else if (parse_u32(t[1], a)) {
                breakpoints.insert(a);
                printf("Breakpoint en 0x%08x.\n", a);
            } else puts("Direccion invalida.");
        } else if (cmd == "delete") {
            u32 a;
            if (t.size() > 1 && parse_u32(t[1], a)) breakpoints.erase(a);
            else puts("Uso: delete addr");
        } else if (cmd == "reset") {
            cpu.reset();
            puts("CPU reiniciada.");
        } else if (cmd == "load") {
            if (t.size() < 2) puts("Uso: load archivo.bin");
            else if (!load_program(cpu, t[1])) puts("No se pudo abrir el archivo.");
        } else {
            printf("Comando desconocido: %s (usa 'help')\n", cmd.c_str());
        }
    }
    return 0;
}