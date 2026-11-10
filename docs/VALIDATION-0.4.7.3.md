# WebCom 0.4.7.3 validation

This hotfix is based directly on the submitted Windows MSVC build log. The
reported failures occurred while parsing WebView2/GDI+ before application code
was reached. The native include environment has been corrected accordingly.

Portable model/test suites are run on this Linux host. The host still cannot
run MSVC or WebView2, so the definitive Windows acceptance step is to run
`build.cmd` on Windows. If a later application-code error remains,
`build-last.log` will contain it after the header-parsing errors are removed.
