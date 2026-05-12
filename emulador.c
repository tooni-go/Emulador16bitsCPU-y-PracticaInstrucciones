/*
 * LC-6 (16 bits, Harvard): emulador interactivo con monitor tipo consola de hardware.
 * Compilar: gcc -std=c11 -Wall -Wextra -O2 -o lc6 emulador.c
 */
#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define PROG_MEM_SIZE 4096u
#define DATA_MEM_SIZE 4096u

/* PSW: bit 0 = Z (ADD result == 0), bit 1 = C (carry sin signo en ADD) */
#define PSW_Z 0x0001u
#define PSW_C 0x0002u

/* Extensión del ISA para el monitor: parada explícita en modo run */
#define OP_HLT 0xFu

typedef struct CPU_State {
    uint16_t program_memory[PROG_MEM_SIZE];
    uint16_t data_memory[DATA_MEM_SIZE];
    uint16_t regs[16];
    uint16_t pc;
    uint16_t psw;
} CPU_State;

typedef enum StepResult {
    STEP_ERR = 0,
    STEP_OK = 1,
    STEP_HALT = 2
} StepResult;

/*
 * Mapa: 0-3 t0-t3, 4-9 s0-s5, 10 ra, 11 gp, 12 sp, 13 fp, 14-15 a0-a1
 */
static const char *reg_name(unsigned r) {
    static const char *names[] = {
        "t0", "t1", "t2", "t3",
        "s0", "s1", "s2", "s3", "s4", "s5",
        "ra", "gp", "sp", "fp", "a0", "a1"
    };
    return (r < 16u) ? names[r] : "?";
}

static int reg_index_from_name(const char *s) {
    if (!s || !*s) {
        return -1;
    }
    for (unsigned i = 0; i < 16u; i++) {
        if (strcasecmp(s, reg_name(i)) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int16_t sign_extend_4(uint16_t imm4) {
    imm4 &= 0xFu;
    if (imm4 & 8u) {
        return (int16_t)(imm4 | 0xFFF0u);
    }
    return (int16_t)imm4;
}

static void psw_set_add_result(CPU_State *cpu, uint32_t sum32, uint16_t result16) {
    if (result16 == 0u) {
        cpu->psw |= PSW_Z;
    } else {
        cpu->psw &= (uint16_t)~PSW_Z;
    }
    if (sum32 > 0xFFFFu) {
        cpu->psw |= PSW_C;
    } else {
        cpu->psw &= (uint16_t)~PSW_C;
    }
}

static void print_psw_banner(const CPU_State *cpu) {
    unsigned z = (cpu->psw & PSW_Z) != 0;
    unsigned c = (cpu->psw & PSW_C) != 0;
    printf("  PSW = 0x%04X  |  Z (cero ADD) = %u  |  C (carry ADD) = %u\n",
           cpu->psw, z, c);
}

/* Tabla compacta 4x4 + PC y PSW (andamiaje para beq / ADD) */
void print_register_grid(const CPU_State *cpu) {
    const char *h = "+----------------+----------------+----------------+----------------+";
    printf("%s\n", h);
    for (unsigned row = 0; row < 4u; row++) {
        printf("|");
        for (unsigned col = 0; col < 4u; col++) {
            unsigned i = row * 4u + col;
            printf(" %3s = 0x%04X   |", reg_name(i), cpu->regs[i]);
        }
        printf("\n");
        printf("%s\n", h);
    }
    printf("  PC  = 0x%04X\n", cpu->pc);
    print_psw_banner(cpu);
    printf("\n");
}

void dump_state(const CPU_State *cpu) {
    print_register_grid(cpu);
}

static void trace_fetch(uint16_t pc, uint16_t ir) {
    printf("Fetch: Cargando IR=0x%04X desde program_memory[PC=0x%04X]\n", ir, pc);
}

static void trace_decode(const char *msg) {
    printf("Decode: %s\n", msg);
}

static void trace_execute(const char *msg) {
    printf("Execute: %s\n", msg);
}

void load_program(CPU_State *cpu, uint16_t start_pc,
                  const uint16_t *words, size_t word_count) {
    for (size_t i = 0; i < word_count; i++) {
        uint32_t addr = (uint32_t)start_pc + (uint32_t)i;
        if (addr >= PROG_MEM_SIZE) {
            fprintf(stderr, "load_program: dirección fuera de rango (0x%04X)\n",
                    (unsigned)addr);
            exit(EXIT_FAILURE);
        }
        cpu->program_memory[addr] = words[i];
    }
}

static int mem_addr_ok(uint32_t addr) {
    return addr < DATA_MEM_SIZE;
}

/*
 * Un ciclo completo con trazas opcionales (Fetch / Decode / Execute).
 * HLT (0xF): parada — no modifica PC ni estado (salvo que ya esté en HALT).
 */
StepResult cpu_step(CPU_State *cpu, int trace) {
    if ((uint32_t)cpu->pc >= PROG_MEM_SIZE) {
        if (trace) {
            trace_fetch(cpu->pc, 0);
            trace_decode("PC fuera de program_memory");
            trace_execute("Abort: bus de programa inválido");
        } else {
            fprintf(stderr, "cpu_step: PC fuera de program_memory (PC=0x%04X)\n", cpu->pc);
        }
        return STEP_ERR;
    }

    uint16_t pc_curr = cpu->pc;
    uint16_t ir = cpu->program_memory[pc_curr];
    uint16_t next_pc = (uint16_t)(pc_curr + 1u);

    uint16_t op = (ir >> 12) & 0xFu;
    uint16_t rd = (ir >> 8) & 0xFu;
    uint16_t rs = (ir >> 4) & 0xFu;
    uint16_t rt_or_off = ir & 0xFu;
    uint16_t target12 = ir & 0x0FFFu;

    if (trace) {
        trace_fetch(pc_curr, ir);
    }

    if (op == OP_HLT) {
        char dbuf[96];
        snprintf(dbuf, sizeof dbuf,
                 "Instrucción HLT detectada (opcode 0x%X, parada del procesador)", op);
        if (trace) {
            trace_decode(dbuf);
            trace_execute("Señal de parada: el ciclo no avanza el PC (queda en la instrucción HLT)");
        }
        return STEP_HALT;
    }

    char dbuf[160];
    switch (op) {
    case 0x0u:
        snprintf(dbuf, sizeof dbuf,
                 "Formato R — ADD: Rd=%s(%u), Rs=%s(%u), Rt=%s(%u)",
                 reg_name(rd), rd, reg_name(rs), rs, reg_name(rt_or_off), rt_or_off);
        break;
    case 0x2u:
        snprintf(dbuf, sizeof dbuf,
                 "Formato I — LW: Rd=%s(%u), Rs=%s(%u), Offset=%u (inmediato de 4 bits)",
                 reg_name(rd), rd, reg_name(rs), rs, rt_or_off);
        break;
    case 0x3u:
        snprintf(dbuf, sizeof dbuf,
                 "Formato I — SW: fuente Rd=%s(%u), base Rs=%s(%u), Offset=%u",
                 reg_name(rd), rd, reg_name(rs), rs, rt_or_off);
        break;
    case 0x4u: {
        int16_t se = sign_extend_4(rt_or_off);
        snprintf(dbuf, sizeof dbuf,
                 "Formato I — BEQ: Rd=%s(0x%04X), Rs=%s(0x%04X), offset4=%u (sext=%d palabras)",
                 reg_name(rd), cpu->regs[rd], reg_name(rs), cpu->regs[rs], rt_or_off, (int)se);
        break;
    }
    case 0xEu:
        snprintf(dbuf, sizeof dbuf,
                 "Formato J — J: Target=0x%03X (salto absoluto en program_memory)",
                 (unsigned)target12);
        break;
    default:
        snprintf(dbuf, sizeof dbuf, "OpCode 0x%X no reconocido en IR=0x%04X", op, ir);
        if (trace) {
            trace_decode(dbuf);
            trace_execute("Abort: instrucción ilegal");
        } else {
            fprintf(stderr, "OpCode no implementado: 0x%X (instr 0x%04X en PC 0x%04X)\n",
                    op, ir, pc_curr);
        }
        return STEP_ERR;
    }

    if (trace) {
        trace_decode(dbuf);
    }

    char ebuf[224];

    switch (op) {
    case 0x0u: {
        uint16_t rt = rt_or_off;
        uint16_t vrs = cpu->regs[rs];
        uint16_t vrt = cpu->regs[rt];
        uint32_t sum = (uint32_t)vrs + (uint32_t)vrt;
        uint16_t res = (uint16_t)sum;
        cpu->regs[rd] = res;
        psw_set_add_result(cpu, sum, res);
        unsigned z = (cpu->psw & PSW_Z) != 0;
        unsigned c = (cpu->psw & PSW_C) != 0;
        snprintf(ebuf, sizeof ebuf,
                 "ALU: %s(%u)=0x%04X + %s(%u)=0x%04X => 0x%04X en %s(%u); "
                 "PSW actualizado (Z=%u, C=%u)",
                 reg_name(rs), rs, vrs, reg_name(rt), rt, vrt, res, reg_name(rd), rd, z, c);
        break;
    }
    case 0x2u: {
        uint16_t off = rt_or_off;
        uint16_t base = cpu->regs[rs];
        uint32_t addr = (uint32_t)base + (uint32_t)off;
        if (!mem_addr_ok(addr)) {
            snprintf(ebuf, sizeof ebuf,
                     "Dirección efectiva 0x%04X inválida (fuera de data_memory)",
                     (unsigned)addr);
            if (trace) {
                trace_execute(ebuf);
            } else {
                fprintf(stderr, "lw: dirección de datos inválida 0x%04X\n", (unsigned)addr);
            }
            return STEP_ERR;
        }
        uint16_t memv = cpu->data_memory[addr];
        cpu->regs[rd] = memv;
        snprintf(ebuf, sizeof ebuf,
                 "Dirección efectiva 0x%04X = %s(0x%04X) + offset %u; "
                 "bus de datos: lectura 0x%04X cargada en %s(%u)",
                 (unsigned)addr, reg_name(rs), base, off, memv, reg_name(rd), rd);
        break;
    }
    case 0x3u: {
        uint16_t off = rt_or_off;
        uint16_t base = cpu->regs[rs];
        uint32_t addr = (uint32_t)base + (uint32_t)off;
        if (!mem_addr_ok(addr)) {
            snprintf(ebuf, sizeof ebuf,
                     "Dirección efectiva 0x%04X inválida (fuera de data_memory)",
                     (unsigned)addr);
            if (trace) {
                trace_execute(ebuf);
            } else {
                fprintf(stderr, "sw: dirección de datos inválida 0x%04X\n", (unsigned)addr);
            }
            return STEP_ERR;
        }
        uint16_t val = cpu->regs[rd];
        cpu->data_memory[addr] = val;
        snprintf(ebuf, sizeof ebuf,
                 "Dirección efectiva 0x%04X = %s(0x%04X) + %u; "
                 "bus de datos: escritura 0x%04X desde %s(%u)",
                 (unsigned)addr, reg_name(rs), base, off, val, reg_name(rd), rd);
        break;
    }
    case 0x4u: {
        uint16_t off_u = rt_or_off;
        int16_t off = sign_extend_4(off_u);
        if (cpu->regs[rd] == cpu->regs[rs]) {
            uint32_t target = (uint32_t)pc_curr + (uint32_t)(int32_t)off;
            if (target >= PROG_MEM_SIZE) {
                snprintf(ebuf, sizeof ebuf,
                         "BEQ tomado pero PC destino 0x%04X fuera de rango",
                         (unsigned)target);
                if (trace) {
                    trace_execute(ebuf);
                } else {
                    fprintf(stderr, "beq: PC destino fuera de rango (0x%04X)\n", (unsigned)target);
                }
                return STEP_ERR;
            }
            next_pc = (uint16_t)target;
            snprintf(ebuf, sizeof ebuf,
                     "Igualdad %s==%s; salto relativo: PC 0x%04X + sext(%d) => próximo PC 0x%04X",
                     reg_name(rd), reg_name(rs), pc_curr, (int)off, next_pc);
        } else {
            snprintf(ebuf, sizeof ebuf,
                     "No salta: %s(0x%04X) != %s(0x%04X); PC secuencial 0x%04X",
                     reg_name(rd), cpu->regs[rd], reg_name(rs), cpu->regs[rs], next_pc);
        }
        break;
    }
    case 0xEu: {
        if ((uint32_t)target12 >= PROG_MEM_SIZE) {
            snprintf(ebuf, sizeof ebuf, "Destino J 0x%03X fuera de program_memory", (unsigned)target12);
            if (trace) {
                trace_execute(ebuf);
            } else {
                fprintf(stderr, "j: destino fuera de program_memory (0x%04X)\n", (unsigned)target12);
            }
            return STEP_ERR;
        }
        next_pc = target12;
        snprintf(ebuf, sizeof ebuf,
                 "Salto incondicional: PC <= 0x%04X (target de 12 bits)",
                 next_pc);
        break;
    }
    default:
        return STEP_ERR;
    }

    if (trace) {
        trace_execute(ebuf);
    }

    if ((uint32_t)next_pc >= PROG_MEM_SIZE) {
        if (trace) {
            snprintf(ebuf, sizeof ebuf, "PC siguiente 0x%04X fuera de program_memory", next_pc);
            trace_execute(ebuf);
        } else {
            fprintf(stderr, "cpu_step: PC siguiente fuera de rango (0x%04X)\n", next_pc);
        }
        return STEP_ERR;
    }
    cpu->pc = next_pc;
    return STEP_OK;
}

static uint32_t parse_u32(const char *s, int *ok) {
    char *end = NULL;
    if (!s || !*s) {
        *ok = 0;
        return 0;
    }
    unsigned long v = strtoul(s, &end, 0);
    if (end == s || *end != '\0' || v > 0xFFFFFFFFul) {
        *ok = 0;
        return 0;
    }
    *ok = 1;
    return (uint32_t)v;
}

static void cmd_help(void) {
    printf(
        "Comandos LC-6 monitor:\n"
        "  s | step              — una instrucción (Fetch/Decode/Execute + tabla regs)\n"
        "  r | run               — continuo hasta HLT (opcode 0xF) o error\n"
        "  m <addr>              — palabra en data_memory[addr] (hex o decimal)\n"
        "  set <reg> <val>       — escribe registro, ej: set t0 0x00FF\n"
        "  pc                    — muestra PC\n"
        "  pc <val>              — fija PC\n"
        "  q                     — salir\n"
        "Opcode HLT: IR=0xF000 (o cualquier instrucción con nibble alto 0xF).\n\n");
}

static void boot_default_program(CPU_State *cpu) {
    memset(cpu, 0, sizeof(*cpu));
    /* Demo: sw a0,0(t0); lw a1,0(t0); HLT — precarga regs vía 'set' o aquí para prueba rápida */
    const uint16_t prog[] = {
        0x3E00u, /* sw  a0, 0(t0) */
        0x2F00u, /* lw  a1, 0(t0) */
        0xF000u, /* hlt */
    };
    load_program(cpu, 0, prog, sizeof prog / sizeof prog[0]);
    cpu->regs[0] = 16;
    cpu->regs[14] = 0xABCDu;
    cpu->pc = 0;
}

static void repl(CPU_State *cpu) {
    char line[512];

    printf("LC-6 monitor interactivo. Escriba un comando (h = ayuda).\n");
    print_register_grid(cpu);

    for (;;) {
        printf("lc6> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) {
            printf("\n");
            break;
        }

        char *save = NULL;
        char *tok = strtok_r(line, " \t\n\r", &save);
        if (!tok || tok[0] == '#' || tok[0] == '\0') {
            continue;
        }

        if (strcmp(tok, "q") == 0 || strcasecmp(tok, "quit") == 0) {
            break;
        }
        if (tok[0] == 'h' || strcasecmp(tok, "help") == 0) {
            cmd_help();
            continue;
        }

        if (strcmp(tok, "s") == 0 || strcasecmp(tok, "step") == 0) {
            StepResult r = cpu_step(cpu, 1);
            printf("\n");
            print_register_grid(cpu);
            if (r == STEP_ERR) {
                printf("(step detenido por error)\n\n");
            } else if (r == STEP_HALT) {
                printf("(HLT: ejecute 'step' de nuevo para repetir la parada en el mismo PC)\n\n");
            }
            continue;
        }

        if (strcmp(tok, "r") == 0 || strcasecmp(tok, "run") == 0) {
            for (;;) {
                StepResult r = cpu_step(cpu, 1);
                printf("\n");
                if (r == STEP_ERR) {
                    printf("--- run: error ---\n");
                    print_register_grid(cpu);
                    break;
                }
                if (r == STEP_HALT) {
                    printf("--- run: HLT alcanzado ---\n");
                    print_register_grid(cpu);
                    break;
                }
            }
            continue;
        }

        if (strcmp(tok, "m") == 0) {
            char *a = strtok_r(NULL, " \t\n\r", &save);
            int ok = 0;
            uint32_t addr = parse_u32(a, &ok);
            if (!ok || addr >= DATA_MEM_SIZE) {
                printf("Uso: m <addr>  (addr < %u)\n", (unsigned)DATA_MEM_SIZE);
                continue;
            }
            printf("data_memory[0x%04X] = 0x%04X\n\n", (unsigned)addr, cpu->data_memory[addr]);
            continue;
        }

        if (strcasecmp(tok, "set") == 0) {
            char *rname = strtok_r(NULL, " \t\n\r", &save);
            char *vstr = strtok_r(NULL, " \t\n\r", &save);
            int ri = reg_index_from_name(rname);
            int ok = 0;
            uint32_t v = parse_u32(vstr, &ok);
            if (ri < 0 || !ok || v > 0xFFFFu) {
                printf("Uso: set <reg> <val>   regs: t0-t3 s0-s5 ra gp sp fp a0-a1\n\n");
                continue;
            }
            cpu->regs[(unsigned)ri] = (uint16_t)v;
            printf("OK: %s <= 0x%04X (visible en el próximo ciclo)\n\n", reg_name((unsigned)ri), (unsigned)v);
            continue;
        }

        if (strcasecmp(tok, "pc") == 0) {
            char *vstr = strtok_r(NULL, " \t\n\r", &save);
            if (!vstr) {
                printf("PC = 0x%04X\n\n", cpu->pc);
                continue;
            }
            int ok = 0;
            uint32_t v = parse_u32(vstr, &ok);
            if (!ok || v >= PROG_MEM_SIZE) {
                printf("Uso: pc <val>  con val < %u\n\n", (unsigned)PROG_MEM_SIZE);
                continue;
            }
            cpu->pc = (uint16_t)v;
            printf("OK: PC <= 0x%04X\n\n", cpu->pc);
            continue;
        }

        printf("Comando desconocido. ");
        cmd_help();
    }
}

int main(void) {
    CPU_State cpu;
    boot_default_program(&cpu);
    repl(&cpu);
    return 0;
}
