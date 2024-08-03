# README.md

`sox` means [sox.sf.net](http://sox.sf.net)<BR>
`sox_ng` means this hard fork of `sox-14.4.2`<BR>
`SoX` means the Swiss Army Knife of command-line audio processing and its spirit
in any of its incarnations<BR>

The `SoX_ng` project imports, compares and refines bug fixes and new work 
from the 50-odd software distributions that package SoX
and from the plethora of forks on github and elsewhere,
and makes regular releases with a six-monthly cadence
for each of the micro (bug fixes) and minor (new features) releases.
Major releases (non-backwards-compatible changes) are being considered.

## How to get it

`sox_ng` lives at
[codeberg.org/sox_ng/sox_ng](https://codeberg.org/sox_ng/sox_ng)
and is composed of a SoX code base, a wiki and an issue tracker.

To fetch it:
```
git clone https://codeberg.org/sox_ng/sox_ng
cd sox_ng
```
and if you want local copies of the wiki and the issues:
```
git clone https://codeberg.org/sox_ng/sox_ng.wiki wiki
issues/getissues.sh
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
```
This installs it as `sox_ng`, `sox_ng.h`, `libsox_ng` and so on, so as
not to conflict with traditional `sox`. If you want it to work the same
as the original `sox`, use `./configure --enable-replace`

You can edit and commit to the code and the wiki using Codeberg's web interface
or from the command-line. In fact, the command-line is the only way to add
images and attachments to the wiki.

The issues are currently read-only to the command line
and editable only on the Codeberg web site.

## Community
The SoX_ng project has two mailing lists hosted by sourcehut.org:
`u.sox_ng.users@lists.sr.ht` and `u.sox_ng.devel@lists.sr.ht`;
both are for discussion of the `sox_ng` codebase and of the project itself.

Discussion of SoX itself should remain on the sox.sf.net mailing lists.

[`SoX_ng`'s financial accounts](Accounting) are public and
[Bounties] can be offered for specific work.
