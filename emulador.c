#include <stdint.h>
#include <stdio.h>

#define MEM_SIZE 1024 // Tamaño arbitrario para el ejemplo

// --- MEMORIAS (16 bits) ---
uint16_t program_memory[MEM_SIZE];
uint16_t data_memory[MEM_SIZE];

// --- ESTADO DEL PROCESADOR ---
uint16_t pc = 0;      // Program Counter
uint16_t psw = 0;     // Program Status Word (Banderas de estado)
uint16_t regs[16];    // Register File (16 registros de 16 bits)

/* MAPA DE REGISTROS (Según el pizarrón):
 * 0-3:   t0-t3 (Temporales)
 * 4-9:   s0-s5 (Guardados)
 * 10:    ra    (Return Address)
 * 11:    gp    (Global Pointer)
 * 12:    sp    (Stack Pointer)
 * 13:    fp    (Frame Pointer)
 * 14-15: a0-a1 (Argumentos)
 */

void ejecutar_ciclo() {
    // 1. FETCH: Traer la instrucción apuntada por el PC
    uint16_t instr = program_memory[pc];
    pc++; // El PC avanza a la siguiente línea

    // 2. DECODE: Cortamos la instrucción de 16 bits usando corrimientos (>>) y máscaras (&)
    // Suponemos la estructura: [OpCode: 4 bits] [Rd: 4 bits] [Rs: 4 bits] [Resto: 4 bits]
    
    uint16_t opcode = (instr >> 12) & 0x000F; // Extrae los 4 bits más altos
    uint16_t rd     = (instr >> 8)  & 0x000F; // Extrae el registro destino
    uint16_t rs     = (instr >> 4)  & 0x000F; // Extrae el registro fuente base
    
    // El último bloque depende del tipo de instrucción (R o I)
    uint16_t rt_or_offset = instr & 0x000F;   // Extrae los 4 bits más bajos
    
    // 3. EXECUTE: Evaluamos qué instrucción es
    switch(opcode) {
        // --- FORMATO R ---
        case 0x0: // Asumimos que ADD usa el opcode 0
            regs[rd] = regs[rs] + regs[rt_or_offset]; // rt_or_offset actúa como 'rt' aquí
            break;

        // --- FORMATO I ---
        case 0x2: // lw (2h)
            // Dirección = Registro Base (rs) + Offset (rt_or_offset)
            regs[rd] = data_memory[regs[rs] + rt_or_offset];
            break;

        case 0x3: // sw (3h)
            // Se guarda el valor de Rd en la dirección (Rs + Offset)
            data_memory[regs[rs] + rt_or_offset] = regs[rd];
            break;

        case 0x4: // beq (4h)
            if (regs[rd] == regs[rs]) {
                // Si son iguales, el PC salta según el offset
                pc = pc + rt_or_offset; 
                // Nota: el psw (Program Status Word) podría usarse aquí para guardar la bandera Zero.
            }
            break;

        // --- FORMATO J ---
        case 0xE: // j (14 en decimal, 1110 en binario)
            // El pizarrón muestra [1110] [  target  ]
            // El target ocupa los 12 bits restantes
            uint16_t target_address = instr & 0x0FFF; 
            pc = target_address;
            break;

        default:
            printf("Instrucción no reconocida (OpCode: %x)\n", opcode);
            break;
    }
}

void load_test_program() {
    // Ejemplo: LW R1, 2(R0)  -> Cargar en R1 lo que hay en R0 + offset 2
    // Supongamos Opcode LW = 2, Rd = 1, Rs = 0, Offset = 2
    // Binario: 0010 0001 0000 0010 -> 0x2102
    program_memory[0] = 0x2102; 

    // J 10 -> Saltar a la dirección 10
    // Opcode J = 14 (E en hex), Target = 10 (A en hex)
    // Binario: 1110 0000 0000 1010 -> 0xE00A
    program_memory[1] = 0xE00A;
}

void update_flags(uint32_t result) {
    // Bandera de Zero (Z)
    if ((uint16_t)result == 0) psw |= 0x0001; // Bit 0 para Zero
    else psw &= ~0x0001;

    // Bandera de Carry (C) - Si el resultado superó 16 bits
    if (result > 0xFFFF) psw |= 0x0002; // Bit 1 para Carry
    else psw &= ~0x0002;
}

