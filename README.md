Qt In-App Analytics
===================

Copyright (C) 2015 *[Oleksii Serdiuk](https://oleksii.name/)*.


About
-----

**Qt In-App Analytics** library provides API to integrate in-app
analytics into Qt applications. I wrote it for use in my own projects
and currently it supports only [Amplitude Analytics][]. I don't have
plans to add support for other analytics platforms at this moment,
however contrubitions are welcome.


Usage
-----

The library supports both Qt 4 and Qt 5, and is desgined as drop-in.
Just include `qtinappanalytics.pri` into your Qt project file, include
header of the corresponding analytics class into your `.cpp` file, and
use it. See source code for API. Documentation will come eventually.


License
-------

**Qt In-App Analytics** is licensed under BSD 2-Clause License. See
[LICENSE](LICENSE) for full text of the license.


[Amplitude Analytics]: https://amplitude.com/
