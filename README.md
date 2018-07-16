# a2m

Convert ansi art to mirc art.

![screenshot](https://git.trollforge.org/a2m/plain/screenshot.png)

For reference screenshot is iTerm2 with Menlo font ssh'd to a linux
machine attached to a tmux session running irssi.

## Installation

```
make
sudo make install
```

## Usage

```
a2m [options] [input.ans]
	-l n      Crop n lines from the left side.
	-r n      Crop n lines from the right side.
	-n        Disable color output.
	-p        Print unprintable characters.
	-t size   Specify tab size, default is 8.
	-w width  Specify width, default is 80.
```

## Troubleshooting

### IRC Client

Most clients work fine assuming you're using a good font (see below.)
* Mirc has problems with color sequences following 3 byte Unicode characters.
* IRCCloud has incorrect colors.

### Terminal

Don't use reverse video, just make foreground white on black background, but
not bright white, there should be two different whites, bright white and
grayish white, 16 unique colors in all, as shown below:

Color | Regular | Bright
------|---------|-----
Black   | #000000 (0, 0, 0)       | #555555 (85, 85, 85)
Red     | #aa0000 (170, 0, 0)     | #ff5555 (255, 85, 85)
Green   | #00aa00 (0, 170, 0)     | #55ff55 (85, 255, 85)
Yellow  | #aa5500 (170, 85, 0)    | #ffff55 (255, 255, 85)
Blue    | #0000aa (0, 0, 170)     | #5555ff (85, 85, 255)
Magenta | #aa00aa (170, 0, 170)   | #ff55ff (255, 85, 255)
Cyan    | #00aaaa (0, 170, 170 )  | #55ffff (85, 255, 255)
White   | #aaaaaa (170, 170, 170) | #ffffff (255, 255, 255)

### Fonts

Try *Andale Mono* or *Menlo* for Mac.  *Deja Vu Sans* for Linux,
*Lucida Console* for Windows.

If the font you use is missing some of the block drawing unicode
characters its going to borrow them from another font, which
will probably having a different width than the original font.
