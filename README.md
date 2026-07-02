# Trabajo Práctico: Testeo de Instrucciones STX4

En este documento se detallan asos de prueba diseñados para testear el correcto funcionamiento de las instrucciones de la arquitectura de la CPU STX4. Se agruparon instrucciones complementarias en cada caso para verificar no solo el funcionamiento aislado, sino también la interacción entre la ALU, los registros y la memoria.

Las instrucciones testeadas de momento son: `LUI`, `ORI`, `XOR`, `NOR`, `ADD`, `SUB`, `SRA`, `SW`, `SB`, `LHU`, `BEQ`, `BGT`, `SLTI`, `J`, `JR`.

---

# Caso 1: Carga de Constantes y Enmascaramiento Lógico
## Descripción: Qué estoy testeando
Se testea el uso de las instrucciones lógicas y de formato L para cargar un número de 32 bits en dos pasos, aplicarle una máscara usando un OR exclusivo (`XOR`), y luego negar todos sus bits utilizando la operación `NOR` con el registro `$zero`.
## Instrucciones: instrucciones que usé durante el test
`LUI`, `ORI` (Or Immediate), `XOR` (Exclusive Or), `NOR` (Not Or)

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
set [0x00000000] 0x381400FF
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
Anduvo correctamente. Originalmente encontramos un bug en el simulador que generaba una Instrucción Ilegal (CAUSE 3) con `LUI` (Opcode 7), pero la máquina fue actualizada y el bug fue solucionado. Pudimos comprobar que ahora se puede cargar exitosamente una constante en la parte alta y que la instrucción `NOR` funciona perfectamente como un operador NOT lógico.

---

# Caso 2: Aritmética Negativa y Desplazamientos
## Descripción: Qué estoy testeando
Se testea la correcta generación y manipulación de números negativos en complemento a dos, sumas aritméticas y desplazamientos aritméticos a la derecha para corroborar que el bit de signo se propaga adecuadamente.
## Instrucciones: instrucciones que usé durante el test
`SUB` (Subtract), `ADDI` (Add Immediate), `SRA` (Shift Right Arithmetic)

## Precondiciones:
- Resetear el simulador y acomodar el PC (`reset` -> `set pc 0`).
- Setear en el debugger el registro `$10` ($t2) con el valor `4`: `set r10 0x00000004`.
- Setear en el debugger el registro `$11` ($t3) con el valor `0` (o usar `$0`).

**Comandos del Debugger (Telnet):**
```bash
reset
set pc 0x00000000
set r10 0x00000004
set [0x00000000] 0x0014C01D
set [0x00000004] 0x0B1A000A
set [0x00000008] 0x0018E082
```
## Postcondiciones:
- Avanzar la CPU con `step 3`.
- Chequear con el debugger los registros de destino con el comando `registers`:
- Registro `$12`: debe contener `0xFFFFFFFC` (-4).
- Registro `$13`: debe contener `0x00000006`.
- Registro `$14`: debe contener `0xFFFFFFFE` (-2).

## Conclusiones: 
Anduvo correctamente. Inicialmente encontramos que `ADDI` arrojaba una excepción, pero con la nueva actualización del simulador se confirmó su correcto funcionamiento y la suma inmediata se computó a la perfección. Además, comprobamos que `SRA` utiliza el campo `rt` como fuente de datos (al igual que SLL y SRL, demostrando que el manual posee un error tipográfico en la Tabla A.2 donde declara que utiliza `rs`) y que la propagación del bit de signo se mantiene tal como se espera en un desplazamiento aritmético para números negativos.

---

# Caso 3: Acceso asimétrico a Memoria y Endianness
## Descripción: Qué estoy testeando
Se testea la escritura de datos de 32 bits a memoria (Store Word), sobreescritura de un solo byte en la misma base (Store Byte) y posterior lectura asimétrica de media palabra sin extensión de signo (Load Half Unsigned) para corroborar la alineación y el esquema de almacenamiento de bytes de la CPU.
## Instrucciones: instrucciones que usé durante el test
`SW` (Store Word), `SB` (Store Byte), `LHU` (Load Half Unsigned)

## Precondiciones:
- Setear un puntero base en `$15` apuntando a una dirección válida de RAM (ej. 0x0800): `set r15 0x00000800`.
- Setear el dato original de 32 bits en `$16`: `set r16 0x11223344`.
- Setear el byte a sobreescribir en `$17`: `set r17 0x000000FF`.

**Comandos del Debugger (Telnet):**
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
- En el registro `$18` en el debugger se verifica cómo la CPU ensambla la media palabra, resultado esperado es `0x000033FF`.

## Conclusiones: 
Funciona. Los accesos con SW respetaron la alineación de palabra en una dirección múltiplo de 4. El resultado de la combinación de instrucciones SW -> SB -> LHU permitió validar el esquema de almacenamiento de bytes del simulador: al aplicar SB en la dirección base 0x0800, se modificó el byte menos significativo del dato original (0x44 pasó a ser 0xFF), confirmando que los bytes de menor peso se alojan en las direcciones de memoria más bajas.

Asimismo, se verificó que la instrucción LHU (Opcode 13) se ejecutó correctamente tras la última actualización; la CPU recuperó con éxito la media palabra de 16 bits combinando los contenidos de las direcciones 0x0801 y 0x0800 (0x33FF) y completó los 16 bits restantes del registro $18 con ceros, omitiendo la extensión de signo y operando sin cuelgues ni lecturas truncadas.

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

**Comandos del Debugger (Telnet):**
```bash
reset
set pc 0x00000000
set r10 0x00000032
set r11 0x00000000
set r20 0x00000400
set [0x00000000] 0xB2980064
set [0x00000004] 0x83000002
set [0x00000008] 0x9A960001
set [0x0000000C] 0x0AD60064
set [0x00000010] 0x10000100
set [0x00000400] 0x0500000E
```

## Postcondiciones:
- Avanzar la CPU usando `step 5` para recorrer toda la secuencia lógica.
- Leer `$11`: debe mantenerse en `0` (lo cual comprueba que el branch `BGT` funcionó esquivando la suma).
- Leer el `$pc` (Program Counter) final: debe apuntar a `0x00000400` (comprobando que el salto incondicional y el salto por registro se completaron).

## Conclusiones: 
Anduvo sin problemas. La resolución de los saltos relativos en instrucciones tipo I (`BGT`, `BEQ`) funcionan correctamente como offsets de palabras y no de bytes. El salto `J` dividió bien la dirección destino para ajustarse al formato de palabra, y el salto mediante registro `JR` permitió transferir el control a un espacio arbitrario validando los modos de direccionamiento. Aquí integramos nuevamente el `ADDI` gracias a que el bug fue solucionado.
---

# Caso 5: Subrutinas y Saltos con Enlace (Function Calls)
## Descripción: Qué estoy testeando
Se testea el mecanismo de llamada a subrutinas y retorno. Al ejecutar un salto con enlace (`JAL`), la CPU debe guardar automáticamente la dirección de retorno (`PC + 4`) en el registro `$31` (`$ra`). Luego, la subrutina debe poder retornar el control al programa principal leyendo dicho registro.
## Instrucciones: instrucciones que usé durante el test
`JAL` (Jump And Link), `JALR` (Jump And Link Register), `J` (Jump)

## Precondiciones:
- Reiniciar el simulador y acomodar el PC en 0 (`reset` -> `set pc 0x00000000`).
- El código principal se cargará en `0x00000000` y la subrutina se ubicará lejos en memoria, en `0x00000800`.

**Comandos del Debugger (Telnet):**
```bash
reset
set pc 0x00000000
set [0x00000000] 0x18000200
set [0x00000004] 0x10000300
set [0x00000800] 0x07C0000F
```
## Postcondiciones:
- En lugar de correr paso a paso por cantidad, usaremos `step 3`.
- Esto ejecutará: 1) El JAL a la subrutina, 2) El JALR devolviendo el control al main, 3) El J final.
- Chequear el `$pc`: debería terminar en `0x00000C00`.
- Chequear el registro `$31`: debe valer `0x00000004` (la dirección de retorno guardada por el JAL).

---

# Caso 6: Multiplicación, División y Resto (ALU Avanzada)
## Descripción: Qué estoy testeando
Se testean las operaciones aritméticas complejas de la ALU.
## Instrucciones: instrucciones que usé durante el test
`MUL` (Multiply), `DIV` (Divide), `REST` (Remainder)

## Precondiciones:
- Reiniciar el simulador (`reset` -> `set pc 0`).
- Setear `$10` con el valor `20` (`0x00000014`): `set r10 0x00000014`.
- Setear `$11` con el valor `3` (`0x00000003`): `set r11 0x00000003`.

**Comandos del Debugger (Telnet):**
```bash
reset
set pc 0x00000000
set r10 0x00000014
set r11 0x00000003
set [0x00000000] 0x0296C015
set [0x00000004] 0x0314D018
set [0x00000008] 0x0316E01A
```
## Postcondiciones:
- Avanzar con `step 3`.
- Revisar registros:
  - `$12` debe valer `0x0000003C` (60)
  - `$13` debe valer `0x00000003` (3)
  - `$14` debe valer `0x00000000` (0)

---

# Caso 7: Acceso a Memoria de Media Palabra (Half Words) y Shifts Lógicos
## Descripción: Qué estoy testeando
Se testea el almacenamiento y carga en memoria limitados a 16 bits (`Half Words`), y cómo la ALU maneja los desplazamientos de bits lógicos (sin respetar ni extender el signo).
## Instrucciones: instrucciones que usé durante el test
`SH` (Store Half), `LH` (Load Half), `LW` (Load Word), `SLL` (Shift Left Logical), `SRL` (Shift Right Logical)

## Precondiciones:
- Reiniciar el simulador (`reset` -> `set pc 0`).
- Base de memoria `$15` en `0x00000800`.
- Dato de prueba `$16` con el valor `0x8899AABB`.

**Comandos del Debugger (Telnet):**
```bash
reset
set pc 0x00000000
set r15 0x00000800
set r16 0x8899AABB
set [0x00000000] 0x53E00000
set [0x00000004] 0x63E20000
set [0x00000008] 0x43E40000
set [0x0000000C] 0x00233200
set [0x00000010] 0x00234201
```

## Postcondiciones:
- Avanzar con `step 5`.
- Leer `$17`: debe valer `0xFFFFAABB` (ya que `AABB` tiene el MSB en 1, LH extiende el signo).
- Leer `$18`: debe valer `0x0000AABB` (si la RAM estaba en 0 antes del SH).
- Leer `$19`: debe valer `0xFFFAABB0` (se corrió 4 bits a la izquierda, perdiendo una F y sumando un 0).
- Leer `$20`: debe valer `0x0FFFFAAB` (se corrió 4 bits a la derecha, rellenando con un 0 a la izquierda por ser SRL).

---

# Caso 8: Operaciones Lógicas R-Type y SLT
## Descripción: Qué estoy testeando
Se testean comparaciones entre dos registros y la resolución de las operaciones lógicas booleanas AND y OR, fundamentales para cualquier branching avanzado o manipulación de bits.
## Instrucciones: instrucciones que usé durante el test
`SLT` (Set Less Than), `SLTU` (Set Less Than Unsigned), `AND`, `OR`

## Precondiciones:
- Reiniciar el simulador (`reset` -> `set pc 0`).
- Setear `$10` con `-1` (`0xFFFFFFFF`): `set r10 0xFFFFFFFF`.
- Setear `$11` con `10` (`0x0000000A`): `set r11 0x0000000A`.

**Comandos del Debugger (Telnet):**
```bash
reset
set pc 0x00000000
set r10 0xFFFFFFFF
set r11 0x0000000A
set [0x00000000] 0x0296C00C
set [0x00000004] 0x0296D00D
set [0x00000008] 0x0296E008
set [0x0000000C] 0x0296F009
```

## Postcondiciones:
- Avanzar con `step 4`.
- Revisar registros:
  - `$12` debe valer `1`
  - `$13` debe valer `0`
  - `$14` debe valer `0x0000000A`
  - `$15` debe valer `0xFFFFFFFF`
