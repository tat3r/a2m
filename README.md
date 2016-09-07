# a2m

Convert ansi art to mirc art.

## Installation

run make and copy a2m wherever you want.

## Usage

run a2m -h for a list of options

## Troubleshooting

### IRC Client

IRCCloud doesn't work very well.  It's monospaced font is *Iconsolata*, served
up by Google Web Fonts, which doesn't draw the block characters correctly.  In
addition it appears to ignore several lines.  Don't use IRCCloud, it's for
people who habitually make bad decisions in life.  Works best with a terminal
based client.

### Terminal

Don't use reverse video, just make foreground white on black background, but
not bright white, there should be two different whites, bright white and
grayish white, 16 unique colors in all.

### Font

The font you use needs to be monospaced obviously but if its missing some of
the block drawing unicode characters its going to borrow them from another
font, which will probably having a different width, resulting in poor alignment
and jagged edges.  *Andale Mono* is the best I've found so far.
