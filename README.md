# Trabajo Práctico: Testeo de Instrucciones STX4

En este documento se detallan asos de prueba diseñados para testear el correcto funcionamiento de las instrucciones de la arquitectura de la CPU STX4. Se agruparon instrucciones complementarias en cada caso para verificar no solo el funcionamiento aislado, sino también la interacción entre la ALU, los registros y la memoria.

Las instrucciones testeadas de momento son: `LUI` (mediante `ORI/H`), `ORI`, `XOR`, `NOR`, `ADD`, `SUB`, `SRA`, `SW`, `SB`, `LHU`, `BEQ`, `BGT`, `SLTI`, `J`, `JR`.

---

# Caso 1: Carga de Constantes y Enmascaramiento Lógico
## Descripción: Qué estoy testeando
Se testea el uso de las instrucciones lógicas y de formato L para cargar un número de 32 bits en dos pasos, aplicarle una máscara usando un OR exclusivo (`XOR`), y luego negar todos sus bits utilizando la operación `NOR` con el registro `$zero`.
## Instrucciones: instrucciones que usé durante el test
`LUI` (simulada vía `ORI/H`), `ORI` (Or Immediate), `XOR` (Exclusive Or), `NOR` (Not Or)

## Precondiciones:
- Reiniciar el estado de la CPU desde el debugger (`reset`).
- Fijar el Program Counter en 0 (`set pc 0x00000000`).
- Setear el registro `$12` ($t4) con una máscara completa de 1s para el XOR: `set r12 0xFFFFFFFF`.
- Inyectar el código máquina hexadecimal en la memoria (iniciando en la dirección `0x00000000`).

**Code:**
```bash
reset
set pc 0x00000000
set r12 0xFFFFFFFF
set [0x00000000] 0x281500FF
set [0x00000004] 0x2A94AA55
set [0x00000008] 0x0298B00A
set [0x0000000C] 0x02C0D00B
```
## Postcondiciones:
- Avanzar 4 ciclos en la CPU usando `step 4`.
- Ejecutar el comando `registers` para volcar el estado del procesador.
- Leer `$10`: debe contener `0x00FFAA55`.
- Leer `$11`: debe contener `0xFF0055AA`.
- Leer `$13`: debe contener `0x00FFAA55`.

## Conclusiones: 
Anduvo correctamente. Durante las pruebas iniciales descubrimos un problema grave: al intentar ejecutar la instrucción `LUI` de forma nativa (con el Opcode 7, tal como especifica el manual), la máquina arroja una excepción de Instrucción Ilegal (CAUSE 3), lo que indica que `LUI` no fue implementada en esta última versión del emulador. Para poder sortear este bug y cargar la constante de 32 bits, empleamos la instrucción `ORI/H` (Opcode 5) con el bit `h=1` activado (bit 16 del formato L). Esto nos permitió cargar la constante en los 16 bits superiores (MSB) comportándose exactamente igual que un `LUI` legítimo. Adicionalmente, verificamos que la negación mediante `NOR` contra el registro hardwired `$0` funciona perfectamente como un operador NOT lógico.

---

# Caso 2: Aritmética Negativa y Desplazamientos
## Descripción: Qué estoy testeando
Se testea la correcta generación y manipulación de números negativos en complemento a dos, sumas aritméticas y desplazamientos aritméticos a la derecha para corroborar que el bit de signo se propaga adecuadamente.
## Instrucciones: instrucciones que usé durante el test
`SUB` (Subtract), `ORI` (Or Immediate), `ADD` (Add), `SRA` (Shift Right Arithmetic)

## Precondiciones:
- Resetear el simulador y acomodar el PC (`reset` -> `set pc 0`).
- Setear en el debugger el registro `$10` ($t2) con el valor `4`: `set r10 0x00000004`.
- Setear en el debugger el registro `$11` ($t3) con el valor `0` (o usar `$0`).

**Code:**
```bash
reset
set pc 0x00000000
set r10 0x00000004
set [0x00000000] 0x0014C01D
set [0x00000004] 0x281A000A
set [0x00000008] 0x031AD01C
set [0x0000000C] 0x0018E082
```
## Postcondiciones:
- Avanzar la CPU con `step 4`.
- Chequear con el debugger los registros de destino con el comando `registers`:
- Registro `$12`: debe contener `0xFFFFFFFC` (-4).
- Registro `$13`: debe contener `0x00000006`.
- Registro `$14`: debe contener `0xFFFFFFFE` (-2).

## Conclusiones: 
Anduvo. Descubrimos que `ADDI` en el simulador (Opcode 1) arroja una excepción de Instrucción Ilegal (CAUSE 3), por lo que lo reemplazamos exitosamente usando la combinación de `ORI` seguido de un `ADD`. Además, verificamos que `SRA` efectivamente utiliza el campo `rs` en lugar de `rt` como fuente, y comprobamos que la propagación del bit de signo se mantiene tal como se espera en un desplazamiento aritmético para números negativos.

---

# Caso 3: Acceso asimétrico a Memoria y Endianness
## Descripción: Qué estoy testeando
Se testea la escritura de datos de 32 bits a memoria (Store Word), sobreescritura de un solo byte en la misma base (Store Byte) y posterior lectura asimétrica de un byte (Load Byte) para corroborar la alineación, el esquema de almacenamiento y la extensión de signo de la CPU.
## Instrucciones: instrucciones que usé durante el test
`SW` (Store Word), `SB` (Store Byte), `LB` (Load Byte)

## Precondiciones:
- Setear un puntero base en `$15` apuntando a una dirección válida de RAM (ej. 0x0800): `set r15 0x00000800`.
- Setear el dato original de 32 bits en `$16`: `set r16 0x11223344`.
- Setear el byte a sobreescribir en `$17`: `set r17 0x000000FF`.

**Code:**
```bash
reset
set pc 0x00000000
set r15 0x00000800
set r16 0x11223344
set r17 0x000000FF
set [0x00000000] 0x4BE00000
set [0x00000004] 0x5BE20000
set [0x00000008] 0x6BE40000
```
## Postcondiciones:
- Avanzar la CPU con `step 3`.
- Leer `$18` en el debugger para verificar la extensión de signo del byte leído.
- Debería leerse como `0xFFFFFFFF` en `$18`.


## Conclusiones: 
Anduvo. Los accesos con `SW` respetaron la alineación de palabra. Mediante esta prueba, detectamos otro error en el manual: el Opcode `13` (01101) está documentado como `LHU` (Load Half Unsigned), pero la CPU realizó una lectura de 1 solo byte (`Size: 1`) y la extendió con signo (`0xFFFFFFFF`), confirmando que en realidad el Opcode `13` corresponde a la instrucción `LB` (Load Byte).

---

# Caso 4: Bifurcaciones y Condiciones
## Descripción: Qué estoy testeando
Se testea el mecanismo de control de flujo usando saltos condicionales respecto al PC, el registro de comparación con inmediatos, un salto incondicional largo (`J`) y un salto indirecto mediante registro (`JR`).
## Instrucciones: instrucciones que usé durante el test
`SLTI` (Set Less Than Immediate), `BEQ` (Branch Equal), `BGT` (Branch Greater Than), `J` (Jump), `JR` (Jump Register)

## Precondiciones:
- Setear `$10` con el valor `50`: `set r10 0x00000032`.
- Setear `$11` con el valor `0`: `set r11 0x00000000`.
- Setear `$20` con una dirección de memoria lejana (ej 0x0400): `set r20 0x00000400`.

**Code:**
```bash
reset
set pc 0x00000000
set r10 0x00000032
set r11 0x00000000
set r20 0x00000400
set [0x00000000] 0xB2980064
set [0x00000004] 0x83000003
set [0x00000008] 0x9A960002
set [0x0000000C] 0x281A0064
set [0x00000010] 0x02DAB01C
set [0x00000014] 0x10000100
set [0x00000400] 0x0500000E
```

## Postcondiciones:
- Avanzar la CPU usando `step 6` para recorrer toda la secuencia lógica.
- Leer `$11`: debe mantenerse en `0` (lo cual comprueba que el branch `BGT` funcionó esquivando la suma).
- Leer el `$pc` (Program Counter) final: debe apuntar a `0x00000400` (comprobando que el salto incondicional y el salto por registro se completaron).

## Conclusiones: 
Anduvo sin problemas. La resolución de los saltos relativos en instrucciones tipo I (`BGT`, `BEQ`) funcionan correctamente como offsets de palabras y no de bytes. El salto `J` dividió bien la dirección destino para ajustarse al formato de palabra, y el salto mediante registro `JR` permitió transferir el control a un espacio arbitrario validando los modos de direccionamiento. Nuevamente evitamos el uso de `ADDI` debido a los problemas en el emulador, ajustando los offsets de saltos por el aumento de instrucciones.
