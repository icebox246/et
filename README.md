# et - Embeded TODOs

This is a simple TODO manager with a twist.
Its outputs get get saved to markdown file of your choosing, but only take up a defined section.

It uses ncurses for drawing TUI.


## Example TODO list managed by *et*:

<!-- TODOS -->

**TODO (3):**

- [ ] Learn F# and C#
- [ ] Learn OpenGL in Rust
- [ ] Learn WebGL

**DONE (7):**

- [x] Fix readline on windows
- [x] Port ET to windows
- [x] Style this TUI up
- [x] Eat breakfast
- [x] Program in C
- [x] Play video games
- [x] Feed the dog
<!-- ENDTODOS -->

How it looks in the app:

![screenshot](./screenshot.png)

## Get started

Install ncurses (linux) and gcc.

```console

#on linux
./make.sh       

#on windows
./make.bat       

./et README.md

```

## Usage

```console
et <filename>
```

filename - path to your markdown file e.g. README.md

#### Keybindings

- `j`, `DOWN_ARROW` - go down a row
- `k`, `UP_ARROW` - go up a row
- `SPACE`, `ENTER` - toggle done/todo
- `i` - new todo
- `X` - remove item
- `c` - edit item
- `J` - move item down
- `K` - move item up
- `q` - quit et
