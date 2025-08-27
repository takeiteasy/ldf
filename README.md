# ldf

> [!WARNING]
> Work in progress

`ldf` is a **l**isp **d**ata **f**ormat. It's inspired by [sexpline](https://github.com/snmsts/sexpline/), but for C/C++. See [test.ldf](https://github.com/takeiteasy/ld/blob/master/test.ldf) for a sample.

There are two different implementations: see [ld.h](https://github.com/takeiteasy/ld/blob/master/ld.h) and [test.c](https://github.com/takeiteasy/ld/blob/master/test.c) for a more comprehensive example that uses heap allocation and the standard library. Or see [ldzero.h](https://github.com/takeiteasy/ld/blob/master/ldzero.h) and [zero.c](https://github.com/takeiteasy/ld/blob/master/zero.c) for a c89 version that has no dependencies and doesn't allocate anthing.

They are both single header implementations so just `#define LD_IMPLEMENTATION` before including. The zero allocation version is largely based off [JSMN](https://github.com/zserge/jsmn/tree/master), an embeddable descending parser for JSON by [zserge](https://github.com/zserge).

## TODO

- [ ] Exporting (from C) API
- [ ] Convert LDF <-> JSON tool

## LICENSE
```
ldf, lisp data format

Copyright (C) 2025 George Watson

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```
