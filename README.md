smd
===

`smd` is a renderer for _simplified markdown_, a variant of markdown that
can be processed more quickly and easily than regular markdown.

You can write simplified markdown in such a way that it is compatible with
standard markdown, and this intersection captures most of the expressiveness
of markdown.

The implementation is ~500 lines of C and compiles to a ~66KB musl-static
binary. It processes the input in a single pass, buffering only two lines
at a time; it doesn't use malloc at all.

It supports most of the features of markdown including:
* Underline and hash headings (single line only)
* Inline code, indented code, and code fences (backticks only)
* Unordered lists (including nesting, `*` and `-` only)
* Ordered lists (including nesting, but must be a single digit)
* Blockquotes (including nesting)
* Links, autolinks, and images (but title field not supported (for tooltips))
* Bold and italic text (but `*` always means bold and `_` always means italic)
* Horizontal rules (`---` only)
* Inline HTML (including entities)
* Line breaks (backslash only)

It also supports these extensions:
* Mathjax spans and blocks with `$` and `$$`
* Asterisk footnote links (`[^id]` and `[^id]: footnote`)
* Tables (`| a | b |`) (column alignment colons are ignored)
* Description lists (single line only: `= name: value`)
* Aside containers (fenced by `:::`)
* Collapsible details containers (fenced by `+++`, first line is summary)

Markdown features that are not supported:
* Reference-style links (can use footnote links instead)
* Raw HTML blocks

Other implementation differences:
* Inline spans, links, and HTML tags must be contained within a single line
* All inline spans have the same precedence and are processed left to right
* Inline span delimiter runs must be the same length for the start and end
* Leading spaces less than 4 are not removed
* Blockquote `>` must be followed by space or newline
* List items and blockquotes always contain a paragraph tag
* Tabs are not internally supported for container indentation
  * To use tabs for container indentation, use `expand -t 4 <file> | smd`
* Spaces are not trimmed inside spans

Other minor/technical differences:
* Lines are limited to 4094 bytes
* Simpler rules for span delimiter adjacencies
* Blocks are delimited by a prefix rather than a pattern
* Trailing characters after code fences and thematic breaks are discarded
* Autolink processing is simplified
* Does not check for non-ASCII whitespace characters

Usage
-----

Install by running `./make && sudo ./make install`. This requires a C compiler.

Then pass markdown over stdin: `smd < filename.md`
