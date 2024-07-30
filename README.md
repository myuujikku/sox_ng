@ README

`sox` means [sox.sf.net](http://sox.sf.net)<BR>
`sox_ng` means this hard fork of `sox-14.4.2`<BR>
`SoX` means the Swiss Army Knife of command-line audio processing and its spirit
in any of its incarnations<BR>

The SoX_ng project makes regular releases of SoX
with a six-monthly scadence for each of the micro, minor and major releases.

It lives on
[codeberg.org](https:///sox_ng/sox_nghttps://codeberg.org/sox_ng/sox_ng)
and is composed of a SoX code base, an issue tracker and a wiki.

To get it:
```
git clone https://codeberg.org/sox_ng/sox_ng
cd sox_ng
git clone https://codeberg.org/sox_ng/sox_ng,wiki wiki
bin/getissues
```

To compile it:
```
autoreconf -i
./configure
make
```
and to install it:
```
sudo make install
 '''

You can edit and commit the code, which is in C, shell, autoconf
and the wiki which is .md and image files.

The issues are currently read-only from the command line
and editable only on the Codeberg web site.

## Community
The SoX_ng project has two mailing lists on sourcehut.org:
u.sox_ng.users@lists.sr.ht and u.sox_ng.devel@lists.sr.ht,
entrambi for discussion both of `sox_ng's codebase and of the project itself.

Discussion of SoX itself should remain on the sox.sf.net mailing lists.
