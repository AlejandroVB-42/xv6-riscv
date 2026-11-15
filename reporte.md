**Taller**

**Integrantes:**  
**Leonardo Alexis Rosero Mendez-2518313**  
**Alejandro Velez Bejarano-**

1\. **Preparar el repositorio y el entorno**

1.1   
Versión de linux:  
![][image1]  
Identificador commit:  
![][image2]  
1.2.   
![][image3]

**2\. Compilar y ejecutar XV6**  
2.1   
Comando: \`make qemu\`. Se ejecutaron \`ls\`, \`cat README\`. 

Comando make qemu  
La salida muestra la línea de qemu-system-riscv64 el mensaje xv6 kernel is booting y, al final, init: starting sh, que indica que el proceso init arrancó la shell y quedó el prompt \$ esperando comandos.   
![][image4]  
Comando ls  
El comando ls lista todo lo que hay dentro del sistema operativo XV6  y programas de usuario como (cat, echo, grep, ls, sh, wc, usertests, entre otros)   
![][image5]  
Comando cat README  
Con el comando cat README se imprimió todo lo que había dentro del archivo README 

![][image6]  
2.2  
El makefile  es el que define las cpus predeterminadas que por defecto es 3 

![][image7]  
Comando usado: \`make qemu CPUS=1\`   
Evidencia:  
 \- Con el valor por defecto, el comando de QEMU lleva \`-smp 3\` y aparecen \`hart 1   
starting\` y \`hart 2 starting\`.  
 \- Con \`CPUS=1\` el comando lleva \`-smp 1\` y no aparece ningún \`hart N starting\`.  
 \- En el monitor de QEMU (\`Ctrl+a c\`), \`info cpus\` lista solo \`CPU \#0\`.  

![][image8]![][image9]![][image10]

**3\. Investigar el planificador original**  
**3-1** XV6 usa Round Robin (turno rotatorio) con quantum fijo igual a un tick de reloj, sin prioridades. El planificador recorre la tabla de procesos proc\[NPROC\] en orden, escoge el primer proceso RUNNABLE que encuentra, le da la CPU y, cuando el proceso la cede (normalmente por el temporizador), sigue buscando desde la posición siguiente. Todos los procesos listos tienen el mismo trato.

Para verlo escribimos un programa de usuario, rr.c, que crea tres hijos (A, B y C). Cada uno repite un ciclo de cálculo y después imprime su letra. No modifica el planificador, solo lo observa. Lo corrimos con una sola CPU:  
\$ rr  
AAABBBCCCCAAAABBBBCCCAAABBBBCCCCAAAAABBBBCCCCAAABBBBCCCAABCC

Salen 60 letras, 20 de cada una. Las letras aparecen en bloques de 3 a 5 y el orden siempre es A, B, C, A, B, C. Ningún proceso termina antes de que los otros avancen, que es lo que se esperaría de turnos rotatorios.  
Esperábamos ver las letras alternadas una por una, y salieron bloques. Nuestra hipótesis es que una vuelta del ciclo dura menos que un tick, así que cada proceso alcanza a imprimir varias letras antes de que el reloj lo interrumpa. No medimos la duración de las vueltas. Con una ejecución más larga (rr 200 &) los bloques quedaron casi siempre de 4 letras, lo que va con esa hipótesis, pero sigue sin comprobarse.  
Mientras rr 200 & corría, pulsamos Ctrl-p, que imprime el estado de los procesos:

![][image11]  
•Con una sola CPU hay un solo proceso en run y los otros dos hijos en runble.  
•El proceso en run cambia de una captura a otra (7, 5, 7, 6), así que la CPU rota entre los listos.  
•La letra que salió justo antes de cada lista coincide con el proceso en run, lo que nos hace pensar que 5 es A, 6 es B y 7 es C. Es una deducción por coincidencia.  
•El padre (pid 4\) está en sleep porque espera a sus hijos con wait. Con rr ya terminado, Ctrl-p solo mostró init y sh en sleep

### **3.2 Archivo y función de selección**

* **Archivo:** `kernel/proc.c`  
* **Función:** `scheduler(void)`, definida en la línea 429 (comentario explicativo desde la 421). Se invoca desde `kernel/main.c:44`, al terminar de arrancar cada CPU.

c

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

**3.3**   
![][image12]  
Estados (kernel/proc.h:79): enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };. El campo state está en struct proc (proc.h:86).

Entrada a la CPU (scheduler(), proc.c:447-453): el proceso pasa de RUNNABLE a RUNNING, se registra en c-\>proc y se llama a swtch(\&c-\>context, \&p-\>context) (kernel/swtch.S), que guarda el contexto del planificador y carga el del proceso. El planificador conserva el p-\>lock del proceso durante el cambio.  
![][image13]  
Salida de la CPU (sched(), proc.c:480-497): todo proceso que deja la CPU pasa por sched(). Esta función comprueba que el proceso tiene su p-\>lock (línea 485), que no hay otros locks tomados (487), que su estado ya no es RUNNING (489) y que las interrupciones están apagadas (491); después ejecuta swtch(\&p-\>context, \&mycpu()-\>context) (495) para volver al planificador. Por eso, antes de llamar a sched(), el proceso debe haber cambiado su estado:}  
**3.4**   
El código del planificador está en dos archivos: kernel/proc.c (scheduler(), sched(), yield(), sleep(), wakeup()) y kernel/swtch.S (la rutina swtch en ensamblador). Los dos aparecen en la lista OBJS del Makefile (empieza en la línea 4\) como \$K/proc.o (línea \[N1\]) y \$K/swtch.o (línea \[N2\]), junto con trap.o, start.o y los demás objetos del kernel. Si un .o no está en OBJS, no entra al sistema.  
Primero riscv64-linux-gnu-gcc (el compilador cruzado) traduce cada .c y .S a un .o. Por ejemplo, proc.c pasa a kernel/proc.o \[evidencia: salida de make kernel/proc.o\]. Después ld junta todos los objetos en un solo ejecutable y resuelve las referencias entre archivos: proc.c llama a swtch, y ese símbolo lo define swtch.S. Lo hace la regla de las líneas 94-95:  
`$K/kernel: $(OBJS) $K/kernel.ld`  
`$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS)`  
kernel.ld indica en qué direcciones de memoria queda cada sección. El resultado es kernel/kernel, y en \[evidencia: grep \-n "scheduler\\\|swtch" kernel/kernel.sym\] se ve que scheduler y swtch quedaron dentro del binario.  
make qemu depende de \$K/kernel y fs.img, y lanza qemu-system-riscv64 \-kernel kernel/kernel .... La cantidad de CPU virtuales sale de \-smp \$(CPUS) \[línea \[N3\] del Makefile\]. Ya en ejecución, start.c:44 llama a timerinit() y main.c:44 llama a scheduler(), que nunca retorna. Así cada CPU queda en el ciclo del planificador.  
La política está fija en el código de scheduler(): no hay un selector de algoritmos. Para cambiarla hay que editar proc.c, recompilar y volver a ejecutar.

**4\. Describir MLFQ y explorar su incorporación**  
Descripción inicial antes de consultar a la IA  
MLFQ es un algoritmo de planificación con varias colas de prioridad. Lo que  
busca es que los procesos interactivos respondan rápido y que los procesos  
largos igual terminen, sin saber de antemano cuánto va a durar cada uno.  
Las reglas que entendimos:

1\. Si A tiene más prioridad que B, corre A.  
2\. Si tienen la misma prioridad, se turnan con Round Robin.  
3\. Todo proceso nuevo empieza en la cola más alta.  
4\. Si un proceso gasta todo su tiempo en un nivel, baja una cola. Si suelta  
   la CPU antes (por E/S), se queda donde está.  
5\. Cada cierto tiempo, todos los procesos vuelven a la cola más alta para  
   que los de abajo no se queden sin CPU.

**PROMPT 1 JUNTO A DESCRIPCION**  
Actúa como tutor socrático de una pareja de estudiantes de cuarto semestre de Ingeniería  
de Sistemas. Conocemos C++ y hemos comenzado a explorar XV6 para RISC-V.  
Queremos identificar y justificar qué se necesitaría para incorporar MLFQ a XV6. En esta  
entrega no vamos a implementarlo.  
Nuestra descripción inicial del algoritmo, escrita antes de esta conversación:  
MLFQ es un algoritmo de planificación con varias colas de prioridad. Lo que  
busca es que los procesos interactivos respondan rápido y que los procesos  
largos igual terminen, sin saber de antemano cuánto va a durar cada uno.  
Las reglas que entendimos:

1\. Si A tiene más prioridad que B, corre A.  
2\. Si tienen la misma prioridad, se turnan con Round Robin.  
3\. Todo proceso nuevo empieza en la cola más alta.  
4\. Si un proceso gasta todo su tiempo en un nivel, baja una cola. Si suelta  
   la CPU antes (por E/S), se queda donde está.  
5\. Cada cierto tiempo, todos los procesos vuelven a la cola más alta para  
   que los de abajo no se queden sin CPU.  
Dudas:

\- No tenemos claro si el "tiempo en un nivel" es lo mismo que el quantum  
  de cada turno o es un total acumulado.  
\- Si un proceso suelta la CPU justo antes de que termine su turno, ¿se le  
  reinicia el contador? Parece que se podría aprovechar eso.  
\- ¿Cuántas colas se usan y cuánto dura el quantum en cada una?  
\- Si llega un proceso de mayor prioridad mientras otro corre, ¿lo  
  interrumpe de una vez o espera a que acabe su turno?.  
Lo que averiguamos del planificador original y de su incorporación al sistema compilado:  
Lo que averiguamos del planificador original (XV6 RISC-V, commit fa2ddcb,  
con una sola CPU: make qemu CPUS=1):

\- Política: Round Robin, sin prioridades, con quantum de un tick de reloj.  
\- Selección: kernel/proc.c, scheduler() (línea 429). Recorre proc\[NPROC\]  
  (líneas 445-464) y a cada proceso RUNNABLE lo pasa a RUNNING y hace  
  swtch() (línea 453).  
\- Salir de la CPU: todo pasa por sched() (proc.c:480).  
  \- Fin del turno: el reloj llega a usertrap() (trap.c:85-86) y llama a  
    yield() (proc.c:501): RUNNING \-\> RUNNABLE.  
  \- Bloqueo: sleep\_prepare() y sleep() (proc.c:547, 562): RUNNING \-\> SLEEPING.  
    wakeup() (proc.c:576) lo pasa a RUNNABLE.  
  \- Terminación: kexit() (proc.c:326): RUNNING \-\> ZOMBIE.  
\- Reloj: clockintr() (trap.c:167) incrementa ticks y reprograma la siguiente  
  interrupción (línea 179).  
\- Compilación: proc.o y swtch.o están en OBJS del Makefile (líneas 15-16) y  
  se enlazan con kernel.ld en kernel/kernel (Makefile:94-95).

Lo que observamos: con 3 procesos que imprimen A, B y C, la salida rota en  
bloques de unas 4 letras (AAAABBBBCCCC...), y con Ctrl-p solo un proceso  
aparece en "run" y los demás en "runble".

Hipótesis sin comprobar: los bloques de 4 letras se deben a que una vuelta  
del ciclo dura menos que un tick.

Todavía no hemos mirado struct proc (proc.h), allocproc() ni fork().  
Reglas de interacción:  
1\. Formula una sola pregunta principal por turno y espera nuestra respuesta. No presentes  
una lista completa de cambios ni una arquitectura resuelta.  
2\. Empieza examinando nuestra descripción de MLFQ. Ayúdanos a detectar reglas ambiguas,  
ausentes o contradictorias. No sustituyas nuestra descripción por una versión terminada  
ni elijas silenciosamente una variante del algoritmo.  
3\. Pídenos que propongamos y justifiquemos las decisiones que faltan. Después, ayúdanos a  
comprobarlas con ejemplos pequeños de procesos y decisiones de planificación que nosotros  
debamos resolver.  
4\. Guíanos para relacionar cada regla con XV6. Antes de sugerir una modificación,  
pregúntanos qué comportamiento necesitamos y qué evidencia del código respalda nuestra  
propuesta.  
5\. No supongas que tienes acceso a nuestro repositorio. Pide los fragmentos necesarios y  
distingue lo observado de lo que todavía sea una hipótesis. No inventes archivos,  
funciones o comportamientos de nuestra versión.  
6\. No entregues código de implementación, parches, una lista cerrada de archivos por  
modificar ni pseudocódigo que resuelva el planificador. Revisa nuestras propuestas y  
señala problemas mediante preguntas y pistas graduales. Puedes explicar conceptos previos  
con ejemplos mínimos.  
7\. Si nos bloqueamos, reduce la dificultad de la pregunta y ofrece una pista concreta. Si  
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

**Descripción revisada**

\- R1. Prioridad: Si A tiene más prioridad que B, se ejecuta A.

\- R2. Round Robin: Si dos o más procesos tienen la misma prioridad, se turnan mediante el algoritmo Round Robin.

\- R3. Creación de procesos: Todo proceso nuevo entra en la cola de mayor prioridad.

\- R4. Descenso de prioridad: Cada proceso acumula los ticks de CPU que utiliza en su nivel, sumando todos sus turnos, incluso si libera la CPU antes de completar su quantum. Cuando el acumulado alcanza la asignación A del nivel, el proceso desciende una cola. El acumulado no se reinicia cuando el proceso se bloquea; se reinicia cuando cambia de nivel y cuando se aplica el aumento de prioridad de R5.

\- R5. Aumento de prioridad: Cada S ticks, todos los procesos suben a la cola de mayor prioridad y su acumulado vuelve a cero.

Con la R4 original un proceso P que corre casi todo su turno y se bloquea un instante antes del final nunca “gasta todo su tiempo”, así que se queda arriba para siempre, mientras un proceso Q que sí consume todo baja a la última cola y casi no corre. Eso va contra el objetivo de que los interactivos respondan rápido y los largos terminen. Midiendo el uso acumulado, P también baja.

## **Cambios propuestos en XV6**

Suponemos una sola CPU (CPUS=1). Lo marcado como pendiente es algo que todavía no hemos mirado en el código.

| Cambio | Dónde | Para qué y evidencia |
| :---- | :---- | :---- |
| Guardar por proceso el nivel y los ticks acumulados | struct proc (proc.h) | R1, R3 y R4 necesitan saber el nivel y el uso de cada proceso. Hoy sabemos que state está en proc.h:86; el resto de la estructura está pendiente de revisar. |
| Poner el nivel más alto y el acumulado en 0 al crear el proceso | Creación de procesos en proc.c (allocproc/fork, pendiente de confirmar) | R3: todo proceso nuevo entra arriba. Pendiente: todavía no hemos leído estas funciones. |
| Elegir el RUNNABLE de nivel más alto, con Round Robin dentro del nivel | scheduler(), proc.c:445-464 | R1 y R2. Hoy toma el primer RUNNABLE de la tabla (línea 447\) sin mirar prioridad. |
| Cobrar el tick al proceso que corre y decidir si cede o baja de nivel | clockintr() (trap.c:167), usertrap() y kerneltrap() (trap.c:85-86, 157-158), yield() (proc.c:501) | R2 y R4. Hoy cada tick de reloj termina en yield(), o sea que el quantum es siempre 1 tick. Con MLFQ el quantum depende del nivel. |
| No reiniciar el acumulado en sleep ni wakeup | sleep() (562), wakeup() (576) | R4. wakeup() hoy solo limpia p-\>chan y cambia el estado (585-590); hay que cuidar que no toque el acumulado. |
| Subir a todos a la cola más alta cada S ticks | clockintr(), recorriendo proc\[\] con el lock de cada proceso | R5. wakeup() ya recorre proc\[\] tomando el lock de cada proceso (580-593), así que sirve de modelo. |
| Poder ver el nivel de cada proceso | procdump() (el que usa Ctrl-p) | Para comprobar. Ctrl-p ya imprime el estado de cada proceso. |

 

No creemos que haga falta cambiar swtch.S, kernel.ld ni la lista OBJS del Makefile, porque el cambio de contexto y la construcción no dependen de la política.

## **4.4 Decisiones que hay que investigar**

•     Número de colas, quantum por nivel, asignación A y periodo S. Con un quantum de 1 tick quizá los niveles casi no se distingan; habría que ver si conviene cambiar el intervalo del reloj (1000000 en start.c:65).

•     Dónde cobrar el tick (clockintr() o usertrap()) y cómo no contarlo dos veces. En kerneltrap() puede no haber proceso (myproc() \!= 0, línea 157).

•     El reloj solo cuenta ticks enteros. Si P suelta la CPU justo antes de cada interrupción, nunca le cobrarían un tick. No sabemos si un proceso real puede sincronizarse así. Una idea sin comprobar es medir con r\_time() al entrar y salir de la CPU.

•     No vimos el principio de clockintr(), así que no sabemos si ticks se incrementa en todas las CPU o solo en una.

•     Si un proceso de mayor prioridad que se despierta debe quitarle la CPU a otro de inmediato. Hoy XV6 solo vuelve a planificar en un tick, un sleep o un exit.

•     Orden dentro de una cola y después del aumento; y si conviene recorrer proc\[\] o mantener colas, por el costo y por los locks.

•     Si el hijo de fork empieza arriba (R3) o hereda el nivel del padre.

•     Revisar struct proc, allocproc() y fork().

**4.5 Escenarios para comprobar una implementación futura**  
\- Escenario 1 (R1): A se encuentra en un nivel alto y B ya ha descendido a un nivel inferior. Ambos procesos están listos y ejecutándose continuamente, realizando cálculos sin bloquearse. Se espera que B no reciba CPU mientras A siga listo para ejecutarse. B solo podrá avanzar cuando A se bloquee o termine.

\- Escenario 2 (R2): Los procesos A, B y C se encuentran en el mismo nivel de prioridad y están realizando cálculos continuamente. Se espera que la CPU se distribuya de manera equitativa mediante turnos, alternando entre A, B y C. Cada proceso recibe el quantum correspondiente a su nivel.

\- Escenario 3 (R3, R1): El proceso A está realizando cálculos en un nivel bajo y, durante su ejecución, crea un nuevo proceso N mediante fork. Se espera que N comience en el nivel de prioridad más alto y tenga preferencia de ejecución sobre A, ya sea inmediatamente o en el siguiente tick, según lo que se decida.

\- Escenario 4 (R4): El proceso Q realiza cálculos continuamente y nunca se bloquea. Se espera que descienda progresivamente de nivel (Q2 → Q1 → Q0) cuando su tiempo acumulado de ejecución alcance la asignación correspondiente a cada nivel.

\- Escenario 5 (R4): El proceso P realiza cálculos durante casi todo su turno y se bloquea justo antes de finalizarlo, mientras que el proceso Q continúa ejecutándose sin bloquearse. Se espera que P también descienda de nivel, aunque libere la CPU antes de completar su quantum. Con la regla original, este comportamiento no se presentaría.

## **4.6 Conclusiones y retroalimentación de la IA**

•     XV6 usa Round Robin sin prioridades: scheduler() toma el primer RUNNABLE de proc\[\] y cada tick de reloj termina en yield().

•     Para MLFQ habría que guardar por proceso su nivel y su uso acumulado de CPU. Hasta donde hemos visto no hay nada de eso, aunque falta revisar struct proc.

•     Los cambios se concentran en scheduler() (elegir), en el manejo del reloj (cobrar, bajar y subir) y en sleep/wakeup (no reiniciar el acumulado). El cambio de contexto y la construcción del kernel no deberían cambiar.

•     La regla 4 como la escribimos se puede aprovechar soltando la CPU antes del final del turno; por eso medimos el uso acumulado.

•  **Retroalimentación de la IA.**

•     Dijo que habíamos detectado el hueco de la regla 4 por nuestra cuenta y que la duda sobre si “casi todo el turno” cuenta como “todo” era justo el tipo de ambigüedad que buscaba.

•     Nos pidió revisar más adelante el supuesto de que Q solo corre cuando P está bloqueado: depende de cómo se aplican R1 y R2 cuando P vuelve a estar listo y no hay nadie más en su cola. También que anotáramos las 3 colas y el tick por nivel como decisión provisional, sin justificar.

•     Nos hizo corregir el ejemplo de I: sin reinicio al bloquearse, un proceso que vive años acaba bajando, y lo que lo salva es el aumento periódico.

| Intercambio | Pregunta de la IA | Nuestra respuesta | Tipo |
| :---- | :---- | :---- | :---- |
| 1 | Con la regla 4 tal como la escribimos, ¿en qué cola quedan P (suelta la CPU justo antes del final de cada turno de 1 tick) y Q (consume todo su turno) después de 100 turnos? ¿Cómo se reparte la CPU y cumple nuestro objetivo? | P se queda en la cola alta porque nunca gasta “todo” su tiempo; Q baja a la última. Q casi solo corre cuando P está bloqueado. No cumple el objetivo: P en realidad consume CPU y el largo casi no avanza. No supimos decidir si “casi todo el turno” cuenta como “todo”. | Hipótesis |
| 2 | ¿Qué tendría que recordar el sistema de cada proceso, entre un turno y otro, para que P no escape de bajar sin castigar a un interactivo I? | Acumular los ticks que usa en su nivel, no reiniciar al bloquearse y reiniciar al cambiar de nivel y en el aumento. Con A \= 2 ticks, P baja en unos 3 turnos e I necesita unas 20 ráfagas. La IA nos hizo corregir la cuenta de I: solo vale dentro de un periodo S, y lo salva el aumento. | Corrección |
| 3 | ¿Qué momentos del ciclo de vida de un proceso en el planificador original hay que distinguir para sumar los ticks usados, y qué evidencia del código los respalda? | Entrada a la CPU (proc.c:451-453), tick de reloj (trap.c:171 y usertrap 85-86), bloqueo (sleep 562-572 y sched), despertar (wakeup 589-590) y terminación (kexit). Marcamos como hipótesis el problema de que el reloj solo cuenta ticks enteros, y como pendiente struct proc. | Comprobación |

**Conclusiones personales**  
Leonardo Alexis Rosero Mendez  
Entendí que lo difícil de MLFQ es decidir qué se mide para bajar a un proceso: si solo se mira cada turno, un proceso puede soltar la CPU justo antes del tick y nunca bajar, así que hay que acumular el tiempo por nivel y subir a todos periódicamente. Aporté en la revisión de las reglas y en corregir con mi pareja los errores que la IA señaló, y apoyé la búsqueda de las funciones en trap.c y proc.c. Aún necesito estudiar la unidad de r\_time(), cómo adaptar scheduler() a las reglas 1 y 2, y dónde guardar el nivel y el acumulado en struct proc.

Alejandro Velez Bejarano  
Con esta tarea entendí cómo XV6 reparte la CPU, le da turnos a los procesos uno tras otro, y el reloj los interrumpe para que nadie se quede con ella. También vi por qué la regla 4 de MLFQ, como la habíamos escrito, tenía un hueco: un proceso podía soltar la CPU justo antes de que se acabara su turno y nunca bajar de cola, así que hay que medir cuánto ha usado en total. Aporté la configuración del entorno, la lectura del código del planificador junto con mi compañero. Todavía me falta entender mejor cómo se mide el tiempo en el kernel y cómo se mantendrían los turnos dentro de un mismo nivel de MLFQ.  

[image1]: img/image1.png
[image2]: img/image2.png
[image3]: img/image3.png
[image4]: img/image4.png
[image5]: img/image5.png
[image6]: img/image6.png
[image7]: img/image7.png
[image8]: img/image8.png
[image9]: img/image9.png
[image10]: img/image10.png
[image11]: img/image11.png
[image12]: img/image12.png
[image13]: img/image13.png
