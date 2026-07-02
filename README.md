# Trabajo Práctico: Testeo de Instrucciones STX4

En este documento se detallan asos de prueba diseñados para testear el correcto funcionamiento de las instrucciones de la arquitectura de la CPU STX4. Se agruparon instrucciones complementarias en cada caso para verificar no solo el funcionamiento aislado, sino también la interacción entre la ALU, los registros y la memoria.

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

**Code:**
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

**Code:**
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

## Conclusiones:
Funcionó excelente. El salto con enlace (`JAL`) guardó exitosamente la dirección de retorno (`PC + 4`) en `$31` y saltó a la subrutina. Luego, el retorno con `JALR` leyó correctamente ese registro y devolvió el control al programa principal sin problemas, demostrando que el hardware para el manejo de subrutinas está en orden.

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

**Code:**
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

## Conclusiones:
Todo perfecto. Las operaciones aritméticas complejas de la ALU demostraron funcionar correctamente. La multiplicación calculó el producto de forma precisa, y tanto la división como el resto aislaron sus respectivos resultados matemáticos sin sobreescribir ni arrastrar bits extraños entre los registros involucrados.

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

**Code:**
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

**Code:**
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

Conclusiones:
Funciona. El test validó que la ALU del simulador responde de manera exacta a las instrucciones Tipo R (Opcode 0) usando el campo func.Por un lado, se comprobó el manejo de datos con y sin signo: SLT entendió que -1 (0xFFFFFFFF) es menor que 10, dando verdadero (12=1). En cambio, SLTU los comparó como valores sin signo (2^32−1>10), dando falso (13=0).Por el otro, las operaciones lógicas bit a bit corrieron impecable: AND aplicó la máscara de bits aislando el 10, y OR saturó el registro manteniendo el -1 (0xFFFFFFFF). Todo se consolidó en el banco de registros sin fallas en el procesador.

---

# Caso 9: Aritmética y Lógica con Inmediatos
## Descripción: Qué estoy testeando
Se testean las operaciones combinadas con inmediatos (tanto aritméticas como lógicas) para evaluar el formato I, el formato L y las distintas extensiones que realiza la CPU de forma simultánea. 
## Instrucciones: instrucciones que usé durante el test
`ADD` (Add), `ANDI` (And Immediate), `XORI` (Exclusive Or Immediate), `SLTIU` (Set Less Than Immediate Unsigned)

## Precondiciones:
- Reiniciar el simulador (`reset` -> `set pc 0`).
- Setear `$10` con `10` (`0x0000000A`): `set r10 0x0000000A`.
- Setear `$11` con `20` (`0x00000014`): `set r11 0x00000014`.

**Code:**
```bash
reset
set pc 0x00000000
set r10 0x0000000A
set r11 0x00000014
set [0x00000000] 0x0296C01C
set [0x00000004] 0x231A000F
set [0x00000008] 0x335C00FF
set [0x0000000C] 0xBB9E0100
```

## Postcondiciones:
- Avanzar la CPU con `step 4`.
- Revisar registros:
  - `$12` debe valer `0x0000001E` (30)
  - `$13` debe valer `0x0000000E` (14)
  - `$14` debe valer `0x000000F1` (241)
  - `$15` debe valer `1`

## Conclusiones:
Anduvo sin problemas. Las instrucciones inmediatas lógicas (ANDI, XORI) extienden por ceros como se espera, evitando arrastrar signos, y la instrucción SLTIU compara correctamente considerando los números como unsigned, confirmando la decodificación exitosa del formato I y L.

---

# Caso 10: Desplazamientos Dinámicos (Variable Shifts)
## Descripción: Qué estoy testeando
Se testean los desplazamientos a nivel de bit pero utilizando el valor dinámico proveniente de un registro fuente en lugar de un inmediato constante. 
## Instrucciones: instrucciones que usé durante el test
`SLLR` (Shift Left Logical Variable), `SRLR` (Shift Right Logical Variable), `SRAR` (Shift Right Arithmetic Variable)

## Precondiciones:
- Reiniciar el simulador (`reset` -> `set pc 0`).
- Setear la cantidad de desplazamiento en `$10` con `4`: `set r10 0x00000004`.
- Setear el dato a desplazar en `$11` con `0x80000008`: `set r11 0x80000008`.

**Code:**
```bash
reset
set pc 0x00000000
set r10 0x00000004
set r11 0x80000008
set [0x00000000] 0x0296C003
set [0x00000004] 0x0296D004
set [0x00000008] 0x0296E005
```
## Postcondiciones:
- Avanzar con `step 3`.
- Revisar registros:
  - `$12` debe valer `0x00000080`
  - `$13` debe valer `0x08000000` (se metió un cero a la izquierda)
  - `$14` debe valer `0xF8000000` (se propagó el 1 del signo)

## Conclusiones:
Anduvo perfecto. Las instrucciones SLLR, SRLR y SRAR responden correctamente capturando los 5 bits menos significativos del registro especificado y aplicando los corrimientos estipulados sin pérdida de información, probando el control de los shifters dinámicos.

---

# Caso 11: Multiplicación y División Unsigned / High
## Descripción: Qué estoy testeando
Se testea el acceso a la parte alta (`High`) del resultado de la multiplicación y la división aritmética considerando los operandos estrictamente sin signo.
## Instrucciones: instrucciones que usé durante el test
`MULHU` (Multiply High Unsigned), `DIVU` (Divide Unsigned), `RESTU` (Remainder Unsigned)

## Precondiciones:
- Reiniciar el simulador (`reset` -> `set pc 0`).
- Setear `$10` con `0xFFFFFFFF` (4294967295): `set r10 0xFFFFFFFF`.
- Setear `$11` con `2`: `set r11 0x00000002`.

**Code:**
```bash
reset
set pc 0x00000000
set r10 0xFFFFFFFF
set r11 0x00000002
set [0x00000000] 0x0296D017
set [0x00000004] 0x0296E019
set [0x00000008] 0x0296F01B
```
## Postcondiciones:
- Avanzar con `step 3`.
- Revisar registros:
  - `$13` debe valer `1` (el carry-over o high part de la multiplicación de 64 bits)
  - `$14` debe valer `0x7FFFFFFF`
  - `$15` debe valer `1`

## Conclusiones:
Anduvo sin problemas. Las instrucciones inmediatas lógicas (ANDI, XORI) extienden por ceros como se espera, evitando arrastrar signos, y la instrucción SLTIU compara correctamente considerando los números como unsigned, confirmando la decodificación exitosa del formato I y L.

---

# Caso 12: Condiciones de Salto Faltantes
## Descripción: Qué estoy testeando
Se testean todas las lógicas de bifurcación de formato I que no probamos anteriormente. Comprobaremos si saltan o no esquivando incrementos de registros.
## Instrucciones: instrucciones que usé durante el test
`BNE` (Branch Not Equal), `BLT` (Branch Less Than), `BLE` (Branch Less or Equal), `BGE` (Branch Greater or Equal)

## Precondiciones:
- Reiniciar el simulador (`reset` -> `set pc 0`).
- Setear `$10` con `5`: `set r10 0x00000005`.
- Setear `$11` con `10`: `set r11 0x0000000A`.
- Setear `$12` con `5`: `set r12 0x00000005`.

**Code:**
```bash
reset
set pc 0x00000000
set r10 0x00000005
set r11 0x0000000A
set r12 0x00000005
set [0x00000000] 0x8A960001
set [0x00000004] 0x081A0001
set [0x00000008] 0x92D40001
set [0x0000000C] 0x081C0001
set [0x00000010] 0xA2980001
set [0x00000014] 0x081E0001
set [0x00000018] 0xAA960001
set [0x0000001C] 0x08200001
```
## Conclusiones:
Resultados positivos. La avalancha de saltos demostró que el comparador para las bifurcaciones BNE, BLT, BLE y BGE funciona con precisión al evaluar las distintas condiciones, ignorando la ejecución cuando la condición es falsa y tomando el branch mediante el cálculo PC=PC+4+offset cuando es verdadera.

---

# Caso 13: Cargas de Bytes y Direccionamiento Indexado
## Descripción: Qué estoy testeando
Se testean las cargas de bytes (signada vs no signada) y las instrucciones indexadas (terminadas en X) las cuales utilizan el contenido de dos registros que suman para conformar la dirección de memoria.
## Instrucciones: instrucciones que usé durante el test
`LB` (Load Byte), `LBU` (Load Byte Unsigned), `LBX` (Load Byte Indexed), `LWX` (Load Word Indexed)

## Precondiciones:
- Reiniciar el simulador (`reset` -> `set pc 0`).
- Puntero base `$15` en `0x0800`: `set r15 0x00000800`.
- Dato de prueba `$10` con `0x8899AABB`: `set r10 0x8899AABB`.
- Indice de memoria `$14` con `2`: `set r14 0x00000002`.

**Code:**
```bash
reset
set pc 0x00000000
set r15 0x00000800
set r10 0x8899AABB
set r14 0x00000002
set [0x00000000] 0x4BD40000
set [0x00000004] 0x73D60000
set [0x00000008] 0x7BD80000
set [0x0000000C] 0x03DCD012
set [0x00000010] 0x03E00014
```

## Postcondiciones:
- Avanzar con `step 5`.
- Revisar registros:
  - `$11` debe ser `0xFFFFFFBB`
  - `$12` debe ser `0x000000BB`
  - `$13` debe ser `0xFFFFFF99` 
  - `$16` debe ser `0x8899AABB`

## Conclusiones:
Anduvo impecable. Las lecturas de memoria a nivel de byte lograron procesarse correctamente con su respectiva extensión de signo (LB) y cero (LBU). Adicionalmente, las operaciones con modo de direccionamiento indexado (LBX, LWX) calcularon correctamente la dirección efectiva sumando ambos registros (base + offset dinámico) permitiendo abstraerse de los inmediatos estáticos.
