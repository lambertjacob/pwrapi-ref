Hardware Profile for CAC Node 071:

CPU specifications:
```
Architecture:          x86_64
CPU op-mode(s):        32-bit, 64-bit
Byte Order:            Little Endian
CPU(s):                24
On-line CPU(s) list:   0-23
Thread(s) per core:    1
Core(s) per socket:    12
Socket(s):             2
```

`pwrls` output:
```
name=`plat` type=Platform: Energy
    name=`plat.node0` type=Node: Energy
        name=`plat.node0.socket0` type=Socket: Energy
            name=`plat.node0.socket0.core0` type=Core: Power Energy
```

Power Capping Framework Plugin `pwrls` output:
```
name=`plat` type=Platform: Energy
    name=`plat.node0` type=Node: Energy
        name=`plat.node0.memory0` type=Memory: Energy
        name=`plat.node0.memory1` type=Memory: Energy
        name=`plat.node0.socket0` type=Socket: Energy
```

`pwrgen` output (to hwloc.xml):
```
<System>
    <Plugins/>
    <Devices/>
    <Objects>
        <obj name="plat" type="Platform">
            <children/>
        </obj>
    </Objects>
</System>
```
