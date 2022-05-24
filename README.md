A plugin for rofi that emulates [blezz](https://github.com/Blezzing/blezz) behaviour.

To run this you need an up to date checkout of rofi git installed.

Run rofi like:

```
    rofi -show blezz -modi blezz.so -auto-select -matching normal
```

## Commandline Options

The following commandline options are added to rofi:

* `-blezz-config`: Specify blezz config file.
* `-blezz-directory`: Specify blezz start directory.
