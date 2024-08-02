# Rules for the sox_ng wiki

The "master copy" of the sox_ng wiki, if there is such a thing,
lives on Codeberg.

You can browse it online at `https://codeberg.org/sox_ng/sox_ng/wiki`
and fetch a copy by
```
git clone https://codeberg.org/sox_ng/sox_ng.wiki wiki
```
One usually clones it onto the `wiki` subdirectory of a clone of `sox_ng`.

The command-line interface is the only way to add images and attachments
to the wiki.

## HTML version

In the `wiki` directory there is a script `makehtml.sh`. If you run it,
it creates `index.html` and an HTML page for each page of the wiki.
If there is a `Home.md` page, this will become `index.html`
as well as `Home.html`. #79

Internal wikilinks should be coded as `[Accounting](Accounting)`
for the HTML encoding to work properly.
