# Exploración de la planificación de procesos en XV6

**Curso:** Sistemas Operativos
**Integrantes:** `<Nombre 1>` y `<Nombre 2>`
**Repositorio (fork):** `<URL del fork en GitHub>`
**Fecha de entrega:** lunes 16 de noviembre de 2026

> **Cómo usar esta plantilla.** Todo lo marcado con `⚠️ COMPLETAR` es evidencia que solo ustedes pueden producir (versiones reales, commit, capturas, diálogo con la IA, reflexiones). El texto técnico ya está desarrollado, pero **verifíquenlo contra el código de su commit** y reescríbanlo con sus propias palabras: la tarea evalúa "explicación propia". Borren este bloque antes de entregar.

---

## 1. Repositorio, entorno y ejecución

### 1.1 Fork y revisión de partida

| Dato | Valor |
|---|---|
| Repositorio oficial | `https://github.com/mit-pdos/xv6-riscv` |
| Fork del grupo | ⚠️ COMPLETAR |
| Versión de Linux | Ubuntu 26.04.1 LTS (Resolute Raccoon) sobre WSL2 en Windows; kernel 6.18.58.1-microsoft-standard-WSL2 |
| Commit de partida | ⚠️ COMPLETAR (salida de `git rev-parse HEAD` y `git log -1 --oneline`) |

Comandos usados:

```bash
git clone <URL-del-fork> xv6-riscv
cd xv6-riscv
git rev-parse HEAD
cat /etc/os-release
uname -r
```

### 1.2 Herramientas y versiones

XV6 necesita un compilador cruzado para RISC-V (toolchain `riscv64-linux-gnu-` o `riscv64-unknown-elf-`) y el emulador `qemu-system-riscv64`. Instalación típica en Ubuntu/Debian:

```bash
sudo apt install git build-essential gdb-multiarch qemu-system-misc \
     gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

Versiones usadas:

```bash
riscv64-linux-gnu-gcc --version
riscv64-linux-gnu-ld --version
qemu-system-riscv64 --version
make --version
```

| Herramienta | Versión |
|---|---|
| gcc RISC-V | riscv64-linux-gnu-gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0 |
| binutils (ld) | GNU ld (GNU Binutils for Ubuntu) 2.46 |
| qemu-system-riscv64 | QEMU 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3.3) |
| make | GNU Make 4.4.1 |
| git | 2.53.0 |

![Herramientas y versiones](img/herramientas.png)
*⚠️ COMPLETAR: captura de la terminal con las versiones. Verifiquen que la imagen se vea desde GitHub (carpeta `img/` subida al fork).*

### 1.3 Compilar y ejecutar XV6

```bash
make qemu
```

Esto compila el kernel y los programas de usuario, construye `fs.img` y arranca qemu. Dentro de XV6 se probaron `ls`, `cat README`, `echo hola`, etc.

![XV6 operativo](img/xv6-operativo.png)
*⚠️ COMPLETAR: captura mostrando el arranque (`xv6 kernel is booting`) y la salida de `ls` y `cat`. Describan en una o dos frases qué se ve.*

Para salir de qemu: `Ctrl-a` y luego `x`.

### 1.4 Ejecución con una sola CPU virtual

El `Makefile` define el número de CPUs con la variable `CPUS` (por defecto 3) y se la pasa a qemu con la opción `-smp $(CPUS)`. Por eso basta con:

```bash
make qemu CPUS=1
```

Evidencia de que hay una sola CPU:

- Con `make qemu CPUS=1` el arranque no muestra las líneas `hart 1 starting` ni `hart 2 starting` que sí aparecen con `make qemu` (3 CPUs por defecto).
- En el monitor de qemu (`Ctrl-a` y luego `c`), el comando `info cpus` lista una única CPU: `* CPU #0: thread_id=... model=rv64`.
- En el `Makefile` (`grep -n "CPUS" Makefile`): línea 174 `ifndef CPUS`, línea 175 `CPUS := 3` y línea 178 `QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 128M -smp $(CPUS) -nographic`.

![Una sola CPU](img/una-cpu.png)

---

## 2. Planificador original de XV6

> Los fragmentos de esta sección son los del commit de partida del grupo (con número de línea). Si cambian de commit, deben volver a verificarlos.

### 2.1 ¿Qué política utiliza XV6 por defecto?

XV6 usa **Round Robin** (turno rotatorio) con quantum fijo igual a un tick de reloj, sin prioridades. El planificador recorre la tabla de procesos `proc[NPROC]` en orden, escoge el primer proceso `RUNNABLE` que encuentra, le da la CPU y, cuando el proceso la cede (normalmente por el temporizador), sigue buscando desde la posición siguiente. Todos los procesos listos tienen el mismo trato.

**Cómo se evidencia en el comportamiento:**

1. El temporizador interrumpe periódicamente y, si hay un proceso de usuario ejecutándose, `usertrap()` llama a `yield()`. Es decir, ningún proceso retiene la CPU más de un tick.
2. Si se ejecutan varios procesos que consumen CPU a la vez, sus salidas se **intercalan** de forma pareja y ninguno se queda sin ejecutar.

**Prueba realizada.** Se escribió un programa de usuario `user/rr.c` (agregado a `UPROGS` en el `Makefile`) que no modifica el planificador, solo lo observa. El proceso padre crea 3 hijos (A, B y C). Cada hijo repite 20 veces (o las que se pasen como argumento, `rr <n>`): un ciclo de trabajo que consume CPU (`for(volatile int k = 0; k < 10000000; k++);`) y después escribe su letra con `write(1, &names[i], 1)`. El padre espera a los tres con `wait`.

Se ejecutó con una sola CPU virtual (`make qemu CPUS=1`) y dentro de XV6 se corrió `rr`. Salida obtenida:

```text
$ rr
AAABBBCCCCAAAABBBBCCCAAABBBBCCCCAAAAABBBBCCCCAAABBBBCCCAABCC
```

![Alternancia](img/round-robin.png)
*⚠️ COMPLETAR: captura de esta salida.*

**Lo que se observa:**

1. La salida tiene 60 letras: 20 de A, 20 de B y 20 de C, como se esperaba (3 hijos × 20 repeticiones).
2. Las letras aparecen en **bloques** de 1 a 5 letras iguales, y los bloques siguen siempre el orden **A → B → C → A → B → C...**, sin invertirse.
3. Ningún proceso termina completo antes de que los otros hayan avanzado: no se ve `AAAA...BBBB...CCCC...` (que sería ejecución de cada proceso hasta terminar).
4. Al final (`...AABCC`) los procesos van terminando en distinto momento: A termina primero, luego B y al final C.

**Interpretación.** Cada bloque corresponde a un turno de CPU. Con una sola CPU, el proceso en ejecución es interrumpido por el reloj (`usertrap()`, `trap.c:85-86`), llama a `yield()` (`proc.c:501`) y pasa a `RUNNABLE`; entonces `scheduler()` continúa el recorrido de `proc[]` (`proc.c:445`) y da la CPU al siguiente proceso `RUNNABLE` de la tabla. Como los tres hijos ocupan posiciones consecutivas de la tabla, el orden de los turnos es siempre A, B, C, A, B, C. Esto es Round Robin: turnos rotatorios de igual duración, sin prioridades.

**Resultado distinto de lo esperado (y su análisis).** Antes de ejecutar esperábamos ver las letras alternadas una por una (`ABCABC...`). Lo observado fueron bloques de 3 a 5 letras por turno. Nuestra hipótesis es que una vuelta del ciclo de trabajo (10 millones de iteraciones) dura menos que un tick de reloj (el quantum de XV6), así que cada proceso alcanza a escribir varias letras antes de ser interrumpido; la variación del tamaño de los bloques (3, 4 o 5) se debería a que los ticks no coinciden exactamente con el final de cada iteración. Esta explicación es una hipótesis: no medimos la duración de las iteraciones. Se podría comprobar aumentando el tamaño del ciclo: si cada iteración durara más de un tick, los bloques se acercarían a una letra por turno. En cualquier caso, la conclusión sobre la política no depende del tamaño de los bloques: la rotación ordenada y equitativa entre procesos listos es la señal de Round Robin.

Código del programa usado (`user/rr.c`):

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char names[3] = {'A', 'B', 'C'};
  int reps = 20;                    // por defecto 20 repeticiones

  if(argc > 1)
    reps = atoi(argv[1]);           // opcional: rr <repeticiones>

  for(int i = 0; i < 3; i++){
    int pid = fork();
    if(pid < 0){
      printf("fork failed\n");
      exit(1);
    }
    if(pid == 0){
      for(int r = 0; r < reps; r++){
        for(volatile int k = 0; k < 10000000; k++)
          ;                         // trabajo que consume CPU
        write(1, &names[i], 1);     // imprime una letra
      }
      exit(0);
    }
  }
  for(int i = 0; i < 3; i++)
    wait(0);
  write(1, "\n", 1);
  exit(0);
}
```

**Estados de los procesos (`Ctrl-p`).** `Ctrl-p` ejecuta `procdump()` (`kernel/proc.c`) y muestra el pid, el estado y el nombre de cada proceso. Se ejecutó `rr 200 &` (200 repeticiones por hijo, para que dure más) y se pulsó `Ctrl-p` cuatro veces mientras corría. Con `rr` ya terminado solo aparecen `1 sleep init` y `2 sleep sh` (línea base: ambos bloqueados esperando). Con `rr` en ejecución se obtuvo:

```text
$ rr 200 &
$ BBBCCCCAAAABBBBCCCC...BBBBC
1 sleep  init
2 sleep  sh
5 runble rr
4 sleep  rr
6 runble rr
7 run    rr
CCCAAAAABBBB...CCCCAAAA
1 sleep  init
2 sleep  sh
5 run    rr
4 sleep  rr
6 runble rr
7 runble rr
BBBBBCCCCAAAABBBBCCCCAAAABBBBCCCC
1 sleep  init
2 sleep  sh
5 runble rr
4 sleep  rr
6 runble rr
7 run    rr
AAAABBBBCCCCAAAABBBBCCCCAAAAB
1 sleep  init
2 sleep  sh
5 runble rr
4 sleep  rr
6 run    rr
7 runble rr
```

(Las letras son lo que `rr` imprimía entre una pulsación y otra; se abreviaron los tramos largos.)

![procdump](img/procdump.png)
*⚠️ COMPLETAR: captura de esta salida.*

**Interpretación:**

1. **El pid 4 es el proceso padre de `rr`**: está en `sleep` porque llamó a `wait` y espera a sus hijos (en `kexit`, `wakeup(p->parent)`, `proc.c:353`, lo despertará). Los pids 5, 6 y 7 son los tres hijos A, B y C.
2. **Con una sola CPU, en cada instantánea hay exactamente un proceso en `run` y los otros dos hijos en `runble`.** Es el modelo `RUNNING`/`RUNNABLE` del planificador: uno usa la CPU y los listos esperan turno.
3. **El proceso en `run` cambia de una instantánea a otra** (7, 5, 7, 6): la CPU rota entre los listos, no se queda con uno solo.
4. **Coincide con las letras impresas.** En cada instantánea, la última letra impresa justo antes corresponde al proceso que aparece en `run`: terminó en `C` y corría el pid 7; terminó en `A` y corría el 5; terminó en `C` y corría el 7; terminó en `B` y corría el 6. Esto indica que el pid 5 es A, el 6 es B y el 7 es C (es una deducción por coincidencia, no una lectura directa del programa). Además, el orden A (5) → B (6) → C (7) sigue el orden de pids, coherente con el recorrido ordenado de `proc[]` en `scheduler()` (`proc.c:445`).
5. **Tamaño del turno estable.** Con la ejecución larga, casi todos los bloques tienen 4 letras (`AAAABBBBCCCC...`). Esto respalda la hipótesis de arriba: el turno dura un tick y en ese tiempo caben unas 4 vueltas del ciclo; las variaciones ocasionales (3, 5) serían ticks que no coinciden con el final de una iteración. Sigue siendo una explicación probable y no una medición.

Estas dos pruebas (salida de `rr` y `Ctrl-p`) sustentan que XV6 usa Round Robin: turnos rotatorios y de duración fija, sin prioridades, entre los procesos `RUNNABLE`.

### 2.2 Archivo y función de selección

- **Archivo:** `kernel/proc.c`
- **Función:** `scheduler(void)`, definida en la línea 429 (comentario explicativo desde la 421). Se invoca desde `kernel/main.c:44`, al terminar de arrancar cada CPU.

```c
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for (;;) {
    intr_on();
    intr_off();

    int found = 0;
    for (p = proc; p < &proc[NPROC]; p++) {          // proc.c:445
      acquire(&p->lock);
      if (p->state == RUNNABLE) {                    // proc.c:447
        p->state = RUNNING;                          // proc.c:451
        c->proc = p;
        swtch(&c->context, &p->context);             // proc.c:453
        mycpu()->intena = 0;
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if (found == 0) {
      asm volatile("wfi");                           // proc.c:467
    }
  }
}
```

La selección ocurre en el `for` de las líneas 445 a 464: se recorre la tabla `proc[NPROC]` en orden y **cada** proceso `RUNNABLE` que se encuentra recibe la CPU en ese momento. Al volver (cuando el proceso cede la CPU), el recorrido continúa con el siguiente elemento de la tabla. No hay prioridades ni cola de listos: el "turno" lo da la posición en el arreglo. Si no hay ningún proceso `RUNNABLE` (`found == 0`), la CPU espera una interrupción con `wfi`. El `intr_on(); intr_off();` de las líneas 441 y 442 evita un bloqueo cuando todos los procesos esperan y una carrera entre una interrupción y `wfi`.

### 2.3 Relación con los estados y los eventos

Estados (`kernel/proc.h:79`): `enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };`. El campo `state` está en `struct proc` (`proc.h:86`).

**Entrada a la CPU** (`scheduler()`, `proc.c:447-453`): el proceso pasa de `RUNNABLE` a `RUNNING`, se registra en `c->proc` y se llama a `swtch(&c->context, &p->context)` (`kernel/swtch.S`), que guarda el contexto del planificador y carga el del proceso. El planificador conserva el `p->lock` del proceso durante el cambio.

**Salida de la CPU** (`sched()`, `proc.c:480-497`): todo proceso que deja la CPU pasa por `sched()`. Esta función comprueba que el proceso tiene su `p->lock` (línea 485), que no hay otros locks tomados (487), que su estado **ya no** es `RUNNING` (489) y que las interrupciones están apagadas (491); después ejecuta `swtch(&p->context, &mycpu()->context)` (495) para volver al planificador. Por eso, antes de llamar a `sched()`, el proceso debe haber cambiado su estado:

| Evento | Dónde ocurre | Transición del proceso que sale |
|---|---|---|
| Interrupción de reloj (fin del turno) | `usertrap()` en `kernel/trap.c:85-86` y `kerneltrap()` en `kernel/trap.c:157-158` (`which_dev == 2`) → `yield()` en `proc.c:501-508` | `RUNNING → RUNNABLE` (`proc.c:505`) y luego `sched()` (506) |
| Espera de un evento (por ejemplo `wait`) | `sleep()` en `proc.c:562-572`, tras `sleep_prepare(chan)` (547-556) | `RUNNING → SLEEPING` (568) y luego `sched()` (569) |
| Terminación | `kexit(int status)` en `proc.c:326-365` | `RUNNING → ZOMBIE` (358) y luego `sched()` (363) |

Fragmentos relevantes (`proc.c`):

```c
void                                  // 501
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;                // 505
  sched();                            // 506
  release(&p->lock);
}
```

```c
if (which_dev == 2)                   // trap.c:85  (usertrap)
  yield();                            // trap.c:86
```

**Cómo se despierta o se vuelve elegible.** En esta versión `sleep` no recibe el canal como argumento: primero se llama a `sleep_prepare(chan)`, que guarda el canal en `p->chan`, y luego a `sleep()`, que solo pone el proceso en `SLEEPING` si `p->chan` sigue distinto de 0 (`proc.c:567-568`). `wakeup(chan)` (`proc.c:576-595`) recorre `proc[]` y, para cada proceso con `p->chan == chan`, pone `p->chan = 0` y, si estaba `SLEEPING`, lo pasa a `RUNNABLE` (`proc.c:589-590`). Así no se pierde un despertar que llegue antes de que el proceso alcance a dormirse. En `kexit()`, el proceso llama a `wakeup(p->parent)` (`proc.c:353`) para avisar al padre que puede estar en `wait`.

**El temporizador** (`kernel/start.c:54-66`): `timerinit()` habilita la extensión `sstc` (`w_menvcfg(... | MENVCFG_STCE)`, línea 59), permite al modo supervisor usar `time` y `stimecmp` (62) y programa la primera interrupción con `w_stimecmp(r_time() + 1000000)` (65). Cuando `time` alcanza `stimecmp`, llega una interrupción de reloj al kernel y `devintr()` (`trap.c:188`) llama a `clockintr()` (`trap.c:215`). `devintr()` devuelve 2 para este tipo de interrupción (`which_dev == 2`), y eso provoca el `yield()` de `usertrap()` y `kerneltrap()`. En `clockintr()` (`trap.c:167`) ocurre lo siguiente:

```c
acquire(&tickslock);      // trap.c:170
ticks++;                  // trap.c:171
wakeup(&ticks);           // trap.c:172
release(&tickslock);      // trap.c:173
...
w_stimecmp(r_time() + 1000000);   // trap.c:179  (programa el siguiente tick)
```

Es decir, cada tick incrementa la variable global `ticks` (protegida por `tickslock`, declarada en `trap.c:9-10`), despierta a los procesos que duermen sobre `&ticks` (por ejemplo, los que llamaron a `sleep(n)`; ver `sys_sleep` en `sysproc.c:71-88`, que usa `sleep_prepare(&ticks)`) y reprograma la siguiente interrupción 1000000 ciclos de `r_time()` más adelante. Ese intervalo es el **quantum de XV6: un tick**.

Resumen del ciclo:

```
RUNNABLE --(scheduler elige + swtch)--------> RUNNING
RUNNING  --(reloj → yield → sched)----------> RUNNABLE
RUNNING  --(sleep_prepare + sleep → sched)--> SLEEPING --(wakeup)--> RUNNABLE
RUNNING  --(kexit → sched)------------------> ZOMBIE
```

### 2.4 Cómo entra el planificador al sistema compilado

1. `kernel/proc.c` y `kernel/swtch.S` están en la lista `OBJS` del `Makefile` como `$K/proc.o` (línea 15) y `$K/swtch.o` (línea 16), junto con `trap.o`, `start.o` y los demás objetos del kernel.
2. Cada `.c` y `.S` se compila a un `.o` con el compilador cruzado `riscv64-linux-gnu-gcc` (salida de `make`, ver evidencia).
3. El enlazador une todos los objetos con el script `kernel/kernel.ld` (`Makefile:94-95`):
   ```make
   $K/kernel: $(OBJS) $K/kernel.ld
       $(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS)
   ```
   y produce el binario `kernel/kernel`.
4. `make qemu` lo ejecuta con `QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 128M -smp $(CPUS) -nographic` (`Makefile:178`), donde `-smp $(CPUS)` fija el número de CPUs virtuales.
5. En ejecución, `main()` llama a `timerinit()` (`main.c`, vista en `start.c:56`) y `scheduler()` (`main.c:44`); `userinit()` crea el primer proceso y cada CPU queda en el ciclo del planificador.

La política está **fija en el código** de `scheduler()`: no hay un selector de algoritmos. Cambiarla implica editar `proc.c`, recompilar y volver a ejecutar.

**Evidencia de la compilación.** Se ejecutó `make clean`, luego `make kernel/kernel 2>&1 | tee build.log` y `grep -n "proc\|swtch\|kernel.ld" build.log`. En `build.log`:

- Línea 11: `proc.c` se compila a `proc.o` con `riscv64-linux-gnu-gcc -Wall -Werror ... -march=rv64gc -mcmodel=medany -ffreestanding -nostdlib ... -c -o kernel/proc.o kernel/proc.c`.
- Línea 12: `swtch.S` (ensamblador) se compila con `riscv64-linux-gnu-gcc -march=rv64gc -g ... -c -o kernel/swtch.o kernel/swtch.S`.
- Línea 28: el enlace final:

```text
riscv64-linux-gnu-ld -z max-page-size=4096 -T kernel/kernel.ld -o kernel/kernel \
  kernel/entry.o kernel/start.o ... kernel/proc.o kernel/swtch.o ... kernel/trap.o ... kernel/virtio_disk.o
```

Observaciones:

- `proc.o` y `swtch.o` aparecen entre los objetos que se enlazan en `kernel/kernel`, en el mismo orden que la lista `OBJS` del `Makefile`.
- El kernel se compila con `-Wall -Werror`: cualquier advertencia del compilador detiene la construcción. Esto es relevante para una futura implementación, porque un cambio en `proc.c` debe compilar sin advertencias.
- `-ffreestanding -nostdlib` indica que el kernel no usa la biblioteca estándar de C.
- El enlazador mostró la advertencia `kernel/kernel has a LOAD segment with RWX permissions`. No impidió la construcción y es una advertencia conocida del enlazado con binutils recientes; se registra como resultado observado.
- Después del enlace, `make` genera `kernel/kernel.asm` (con `objdump -S`) y `kernel/kernel.sym`. En `kernel/kernel.asm` se puede ubicar la función `scheduler` ya compilada, lo que confirma que el código de `proc.c` forma parte del binario final.

![Compilación del kernel](img/compilacion.png)
*⚠️ COMPLETAR: captura de `grep -n "proc\|swtch\|kernel.ld" build.log` mostrando las líneas 11, 12 y 28.*

### 2.5 Diferencias de esta versión de XV6 frente a la del libro

Conviene dejarlo escrito porque el commit de partida es reciente y difiere de lo que muestra el libro:

- `exit` se llama `kexit` y `printf` del kernel es `printk`.
- `sleep(chan, lk)` pasó a dividirse en `sleep_prepare(chan)` y `sleep()`; `wakeup` limpia `p->chan`.
- El reloj usa la extensión `sstc` (`stimecmp`) y no el CLINT con `timervec` en modo máquina.
- `scheduler()` hace `intr_on(); intr_off();` antes del recorrido.

---

## 3. Exploración de MLFQ

### 3.1 Descripción inicial (escrita antes de consultar a la IA)

> ⚠️ **Esta sección debe ser de ustedes.** Lo siguiente es un borrador de apoyo: léanlo, discutanlo y reescríbanlo con sus palabras y sus dudas reales **antes** de abrir la conversación con la IA. Guarden la versión original.

MLFQ (Multilevel Feedback Queue) mantiene varias colas, cada una con un nivel de prioridad. Reglas que conocemos:

1. Si la prioridad de A es mayor que la de B, se ejecuta A y no B.
2. Si A y B tienen la misma prioridad, se ejecutan por Round Robin.
3. Un trabajo nuevo entra en la cola de mayor prioridad.
4. Si un trabajo consume todo el tiempo asignado en un nivel, baja de prioridad.
5. Cada cierto tiempo S, todos los trabajos suben a la cola superior (para evitar inanición).

**Dudas iniciales:**

- ¿Cuántas colas y cuánto dura el quantum en cada nivel?
- ¿"Tiempo asignado" es el quantum de un turno o una asignación acumulada en el nivel?
- Si un proceso cede la CPU antes de terminar el quantum (por E/S), ¿conserva su nivel? ¿Se reinicia su contabilidad?
- ¿Qué pasa si un proceso de mayor prioridad se vuelve listo mientras otro se ejecuta: se expulsa de inmediato?
- ¿Cómo se elige el valor de S?
- ¿Cómo se desempata dentro de un nivel?

### 3.2 Descripción consolidada y revisada

Tras la interacción con la IA (⚠️ COMPLETAR con lo que realmente concluyan), la variante considerada es la de OSTEP con la regla 4 revisada:

| Regla | Enunciado |
|---|---|
| R1 | Si Prioridad(A) > Prioridad(B), corre A. |
| R2 | Si Prioridad(A) = Prioridad(B), corren en Round Robin con el quantum del nivel. |
| R3 | Todo proceso nuevo entra en el nivel más alto. |
| R4 | Cuando un proceso agota su asignación total de tiempo en un nivel (sin importar cuántas veces haya cedido la CPU), baja un nivel. |
| R5 | Cada S ticks, todos los procesos suben al nivel más alto. |

Parámetros que deben fijarse (propuesta inicial, sujeta a su decisión): 3 niveles (0 = más alto), quantum por nivel de 1, 2 y 4 ticks (asignación igual al quantum en cada nivel), S = 100 ticks.

Se elige la R4 revisada y no las reglas 4a/4b originales porque estas permiten que un proceso conserve su nivel cediendo la CPU justo antes de que termine su quantum ("jugar con el planificador").

### 3.3 Prompt 1 usado con la IA

**Servicio de IA y modelo:** ⚠️ COMPLETAR (por ejemplo, OpenCode con el modelo X).

```text
Actúa como tutor socrático de una pareja de estudiantes de cuarto semestre de Ingeniería
de Sistemas. Conocemos C++ y hemos comenzado a explorar XV6 para RISC-V.
Queremos identificar y justificar qué se necesitaría para incorporar MLFQ a XV6. En esta
entrega no vamos a implementarlo.

Nuestra descripción inicial del algoritmo, escrita antes de esta conversación:
[PEGAR AQUÍ LA DESCRIPCIÓN DE 3.1, CON SUS DUDAS]

Lo que averiguamos del planificador original y de su incorporación al sistema compilado:
- Política: Round Robin con quantum de 1 tick; recorre proc[NPROC] y elige el primer RUNNABLE.
- Selección: kernel/proc.c, función scheduler(), que usa swtch() (kernel/swtch.S).
- Estados y eventos: yield() (RUNNING→RUNNABLE) llamado desde usertrap()/kerneltrap() en
  kernel/trap.c cuando which_dev == 2 (reloj); sleep() (→SLEEPING); wakeup() (→RUNNABLE);
  exit() (→ZOMBIE); todos pasan por sched().
- Construcción: proc.o y swtch.o están en OBJS del Makefile; se enlazan con kernel.ld en
  kernel/kernel; qemu lo carga con -kernel y se limita a 1 CPU con CPUS=1.

Reglas de interacción:
1. Formula una sola pregunta principal por turno y espera nuestra respuesta. No presentes
   una lista completa de cambios ni una arquitectura resuelta.
2. Empieza examinando nuestra descripción de MLFQ. Ayúdanos a detectar reglas ambiguas,
   ausentes o contradictorias. No sustituyas nuestra descripción por una versión terminada
   ni elijas silenciosamente una variante del algoritmo.
3. Pídenos que propongamos y justifiquemos las decisiones que faltan. Después, ayúdanos a
   comprobarlas con ejemplos pequeños de procesos y decisiones de planificación que nosotros
   debamos resolver.
4. Guíanos para relacionar cada regla con XV6. Antes de sugerir una modificación,
   pregúntanos qué comportamiento necesitamos y qué evidencia del código respalda nuestra
   propuesta.
5. No supongas que tienes acceso a nuestro repositorio. Pide los fragmentos necesarios y
   distingue lo observado de lo que todavía sea una hipótesis. No inventes archivos,
   funciones o comportamientos de nuestra versión.
6. No entregues código de implementación, parches, una lista cerrada de archivos por
   modificar ni pseudocódigo que resuelva el planificador. Revisa nuestras propuestas y
   señala problemas mediante preguntas y pistas graduales. Puedes explicar conceptos previos
   con ejemplos mínimos.
7. Si nos bloqueamos, reduce la dificultad de la pregunta y ofrece una pista concreta. Si
   pedimos la solución directa, conserva el modo socrático.
El objetivo es que nosotros logremos:
A. Redactar una descripción revisada y coherente de la variante de MLFQ considerada.
B. Explicar qué cambios necesitaría XV6 para soportarla, indicando para cada uno su
   propósito, su relación con las reglas y la evidencia del código que lo justifica.
C. Proponer escenarios para comprobar una futura implementación del algoritmo MLFQ en el
   sistema operativo XV6. Pídenos que expliquemos qué procesos participarían, qué harían,
   qué regla comprobaríamos y qué comportamiento esperaríamos observar. No nos entregues los
   escenarios resueltos. En esta tarea solo los describiremos; no vamos a programarlos ni
   ejecutarlos.
Mantén el análisis dentro del alcance de una propuesta: no necesitamos implementar MLFQ
ni construir un selector de planificadores. Ayúdanos a registrar las cuestiones que
queden pendientes de investigación.
Al finalizar, pídenos que redactemos nuestras conclusiones. Comprueba su consistencia
mediante una última pregunta aplicada a un caso concreto, sin escribir por nosotros el
reporte.
Comienza con una pregunta que permita comprobar cómo entendemos una regla de nuestra
descripción inicial de MLFQ.
```

> Usen el texto exacto del enunciado, pegado en una **conversación nueva**. Reemplacen los corchetes con su contenido real.

### 3.4 Propuesta de cambios en XV6 (teórica)

Supuesto: una sola CPU (`CPUS=1`), lo que evita estudiar colas por CPU y balanceo de carga. Variante: 3 niveles, R1–R5 de 3.2.

| # | Cambio propuesto | Archivo / función | Necesidad que atiende y por qué |
|---|---|---|---|
| 1 | Agregar a `struct proc` campos para el nivel actual y los ticks consumidos en ese nivel (y, según el diseño, los ticks del turno actual). | `kernel/proc.h` (`struct proc`) | R1, R3, R4: el planificador necesita saber en qué nivel está cada proceso y cuánto ha gastado. Hoy `struct proc` no guarda nada de prioridad ni de tiempo de CPU consumido. Estos campos se protegerían con `p->lock`, siguiendo la convención del archivo. |
| 2 | Inicializar nivel = más alto y contadores = 0 al crear el proceso. | `kernel/proc.c` (`allocproc`, y por herencia `fork`; `userinit` para el primer proceso) | R3: todo proceso nuevo entra en la cola superior. `allocproc` es el punto común de creación. |
| 3 | Reemplazar el criterio "primer `RUNNABLE` de la tabla" por "`RUNNABLE` del nivel más alto, con desempate Round Robin dentro del nivel". | `kernel/proc.c` (`scheduler`) | R1 y R2: la política actual trata a todos igual. Se debe decidir si basta con recorrer `proc[]` buscando el mejor nivel (más simple, ver decisión D1) o mantener colas explícitas (más fiel al algoritmo, más estructura y bloqueos). |
| 4 | Contabilizar el tiempo de CPU del proceso en ejecución en cada tick y decidir si debe ceder la CPU (fin del quantum de su nivel) y si debe bajar de nivel (asignación agotada). | `kernel/trap.c` (`usertrap`, `kerneltrap` y `clockintr`, `trap.c:167`) y/o `kernel/proc.c` (`yield`) | R2 y R4: hoy toda interrupción de reloj llama a `yield()` (quantum fijo de 1 tick). Con MLFQ el quantum depende del nivel, así que no todo tick debe terminar en `yield()`, y alguien debe contar. |
| 5 | Definir qué ocurre con el nivel y la contabilidad cuando un proceso se bloquea y despierta (E/S). | `kernel/proc.c` (`sleep_prepare`, `sleep`, `wakeup`, líneas 547-595) | R4 revisada: la contabilidad debe seguir sumando aunque el proceso ceda antes del quantum, para impedir que se juegue con el planificador. Hay que decidir si `sleep` y `wakeup` conservan o no los contadores. |
| 6 | Decidir si un proceso que pasa a `RUNNABLE` con un nivel mayor que el del que corre lo expulsa de inmediato o espera al siguiente tick. | `kernel/proc.c` (`wakeup`, `kfork` o la función de creación que corresponda; ⚠️ verificar nombre) y `kernel/trap.c` | R1 estricta: sin expulsión, un proceso de alta prioridad puede esperar hasta un tick. XV6 hoy solo replanifica en ticks, `sleep`, `exit`. |
| 7 | Implementar el aumento periódico de prioridad (boost): cada S ticks, poner todos los procesos en el nivel más alto y reiniciar sus contadores. | `kernel/trap.c` (`clockintr()`, `trap.c:167`, que ya incrementa `ticks` bajo `tickslock` en las líneas 170-173) con una función auxiliar en `kernel/proc.c`; declararla en `kernel/defs.h` | R5: evita inanición de procesos de baja prioridad y adapta a procesos que cambian de comportamiento. Requiere recorrer `proc[]` adquiriendo cada `p->lock`. |
| 8 | Proveer observabilidad del nivel de cada proceso (por ejemplo, mostrarlo en `procdump`). | `kernel/proc.c` (`procdump`) | Comprobación futura: sin ver el nivel, solo se puede verificar de forma indirecta. Una syscall nueva (`kernel/syscall.h`, `syscall.c`, `sysproc.c`, `user/usys.pl`, `user/user.h`) sería opcional y debe justificarse. |
| 9 | **No** hace falta modificar: `kernel/swtch.S`, `kernel/kernel.ld`, la lista `OBJS` del `Makefile` (si no se agregan archivos). | — | El cambio de contexto y el proceso de construcción no dependen de la política. |

**Orden de las modificaciones respecto al ciclo de vida:** creación (2) → tick de reloj (4, 7) → decisión de ceder (4) → elección (3) → bloqueo/despertar (5, 6).

### 3.5 Decisiones que requieren investigación o respaldo adicional

| Id | Decisión pendiente | Qué hay que averiguar |
|---|---|---|
| D1 | ¿Recorrer `proc[]` en cada decisión o mantener colas (listas enlazadas) por nivel? | Costo para `NPROC = 64`; cómo mantener el orden Round Robin y el desempate; cómo se protegen las listas con los locks (el orden de adquisición de `p->lock` y evitar *deadlock*). |
| D2 | Valores de los parámetros: número de niveles, quantum por nivel, asignación, S. | Qué valores recomienda OSTEP; relación con el tick de XV6 (`timerinit` en `kernel/start.c:65` programa 1000000 ciclos de `r_time()`; ⚠️ verificar a cuántos ms equivale en qemu), y `clockintr()` lo reprograma en `trap.c:179`). Con quantum de 1 tick, los niveles quizá no se distingan mucho: ¿hay que cambiar el intervalo del reloj? |
| D3 | Dónde se contabiliza el tiempo (en `clockintr`, en `usertrap` o en `yield`) y cómo evitar contar dos veces. | Flujo exacto de `trap.c` y qué usa cada ruta (usuario vs. kernel). |
| D4 | Expulsión inmediata al despertar un proceso de mayor prioridad. | Si es seguro invocar `yield` en esos puntos y cómo interactúa con los locks. |
| D5 | Tratamiento de `sleep`/`wakeup` frente a la contabilidad (R4 revisada). | Que `wakeup` no reinicie contadores por accidente. |
| D6 | Heredar nivel en `fork`: ¿el hijo empieza arriba (R3) o hereda? | Decisión de diseño; R3 indica que arranca en el nivel más alto. |
| D7 | Uso de varias CPU (`CPUS > 1`). | Fuera del alcance de esta tarea; se asume `CPUS=1`. |

### 3.6 Escenarios de comprobación futura (solo descripción; no se programan ni ejecutan)

Parámetros supuestos: 3 niveles (L0 el más alto), quantum 1/2/4 ticks, S = 100 ticks. Estos valores deben ajustarse a la decisión final.

**Escenario 1: prioridad estricta (R1)**
- *Procesos:* A (siempre en L0, intensivo en CPU corto) y B (ya bajado a L1).
- *Qué hacen:* ambos intensivos en CPU y siempre listos.
- *Regla:* R1.
- *Esperado:* mientras A está `RUNNABLE` en un nivel superior, B no se ejecuta. B solo corre cuando A se bloquea o termina.

**Escenario 2: Round Robin en un mismo nivel (R2)**
- *Procesos:* A, B y C, con el mismo nivel.
- *Qué hacen:* los tres intensivos en CPU y con asignación suficiente para no bajar durante la observación.
- *Regla:* R2.
- *Esperado:* se alternan A, B, C, A, B, C, con un quantum del tamaño del nivel y sin que uno monopolice la CPU.

**Escenario 3: llegada de un proceso nuevo (R3 y R1)**
- *Procesos:* A (intensivo, ya en L2) y N (nuevo, creado con `fork` mientras A corre).
- *Qué hacen:* A calcula sin parar; N es intensivo.
- *Regla:* R3 y R1.
- *Esperado:* N inicia en L0 y se ejecuta antes que A (inmediatamente o en el siguiente tick, según la decisión D4).

**Escenario 4: degradación de un proceso intensivo en CPU (R4)**
- *Procesos:* A, un solo proceso intensivo en CPU con otro proceso corto C en L0 para comparar.
- *Qué hace:* A no se bloquea nunca.
- *Regla:* R4.
- *Esperado:* A agota la asignación de L0 y baja a L1; luego agota L1 y baja a L2, donde se queda. El tiempo de CPU consumido en cada nivel coincide con la asignación definida.

**Escenario 5: proceso interactivo (E/S) conserva su prioridad**
- *Procesos:* I (interactivo) y A (intensivo).
- *Qué hacen:* I usa muy poca CPU y duerme con frecuencia (por ejemplo, esperando `sleep(n)`); A no se bloquea.
- *Regla:* R4 (asignación acumulada) y R1.
- *Esperado:* I se mantiene en un nivel alto porque no agota su asignación; cada vez que despierta tiene preferencia sobre A. A baja de nivel.

**Escenario 6: intento de engañar al planificador (R4 revisada)**
- *Procesos:* G (tramposo) y A (intensivo normal).
- *Qué hace G:* cede la CPU voluntariamente poco antes de terminar cada quantum, y A consume todo el quantum.
- *Regla:* R4 revisada.
- *Esperado:* como el consumo de G se acumula aunque ceda, G también baja de nivel con el tiempo. No retiene la prioridad alta indefinidamente.

**Escenario 7: inanición y boost (R5)**
- *Procesos:* L (intensivo, ya en L2) y varios procesos intensivos que se mantienen en L0 y L1 durante un tiempo largo.
- *Qué hacen:* todos listos casi permanentemente.
- *Regla:* R5.
- *Esperado:* sin boost, L no se ejecutaría. Cuando transcurren S ticks, L vuelve a L0 y recibe CPU; después los procesos intensivos vuelven a bajar.

**Escenario 8: cambio de comportamiento (R5)**
- *Procesos:* P, que primero es intensivo en CPU y después pasa a ser interactivo.
- *Regla:* R5 (y R4).
- *Esperado:* P baja durante la fase intensiva; tras el boost sube y, al volverse interactivo, se mantiene alto.

Para cada escenario se observaría el nivel de cada proceso (decisión 8 de 3.4) y el orden de ejecución.

### 3.7 Lo aprendido y retroalimentación de la IA

**Conclusiones del grupo:** ⚠️ COMPLETAR con sus propias palabras. Puntos que deberían aparecer si fueron los que concluyeron:

- XV6 trata a todos los procesos igual (Round Robin, quantum de 1 tick); MLFQ necesita guardar información por proceso (nivel y consumo) que hoy no existe.
- La mayor parte del cambio está en `scheduler()`, la contabilización del tiempo en el manejo de interrupciones de reloj, y los eventos `sleep`/`wakeup`/`fork`.
- El mecanismo de cambio de contexto (`swtch`) no cambia, solo la política.
- Hay decisiones que no pueden cerrarse con el código y deben respaldarse (D1–D7).

**Retroalimentación de la IA:** ⚠️ COMPLETAR (qué observaciones o correcciones les hizo y qué cambiaron a partir de ellas).

**Resultado inesperado u honesto:** ⚠️ COMPLETAR si hubo algo que no salió como esperaban (por ejemplo, un fragmento de código distinto al que creían) y sus posibles causas y límites.

---

## 4. Evidencia del aprendizaje con IA

**Servicio y modelo:** ⚠️ COMPLETAR

### 4.1 Intercambios seleccionados (de 3 a 5)

> Cada uno debe ser una pregunta de la IA y su respuesta de ustedes. Deben incluir al menos una **hipótesis**, una **comprobación** y una **corrección**.

**Intercambio 1 (hipótesis)**
- IA: ⚠️ COMPLETAR
- Respuesta del grupo: ⚠️ COMPLETAR

**Intercambio 2 (comprobación)**
- IA: ⚠️ COMPLETAR
- Respuesta del grupo: ⚠️ COMPLETAR

**Intercambio 3 (corrección)**
- IA: ⚠️ COMPLETAR
- Respuesta del grupo: ⚠️ COMPLETAR

**Intercambio 4 (opcional)**
- IA: ⚠️ COMPLETAR
- Respuesta del grupo: ⚠️ COMPLETAR

**Intercambio 5 (opcional)**
- IA: ⚠️ COMPLETAR
- Respuesta del grupo: ⚠️ COMPLETAR

### 4.2 Reflexión individual

**`<Nombre 1>`:** ⚠️ COMPLETAR (un párrafo: qué comprendió, qué aportó y qué cuestión aún necesita estudiar).

**`<Nombre 2>`:** ⚠️ COMPLETAR (un párrafo: qué comprendió, qué aportó y qué cuestión aún necesita estudiar).

---

## 5. Lista de verificación antes de entregar

- [ ] Fork público (o accesible al profesor) y `reporte.md` en la raíz del repositorio.
- [ ] Nombres de ambos integrantes y enlace al repositorio al inicio.
- [ ] Versión de Linux, commit y versiones de herramientas con imagen.
- [ ] Evidencia de XV6 operativo y de una sola CPU.
- [ ] Evidencia del Round Robin con rutas, funciones y fragmentos (2.1 a 2.4).
- [ ] Descripción inicial, prompt 1 completo y descripción revisada de MLFQ.
- [ ] Propuesta de cambios, decisiones por investigar y escenarios.
- [ ] De 3 a 5 intercambios con la IA y una reflexión por integrante.
- [ ] Las imágenes se ven desde GitHub (carpeta `img/` subida y rutas relativas correctas).
- [ ] No se implementó MLFQ.
