# Installation:

`make && sudo make install`


# Testing

Testing is done using `pytest` and `tox`.

```bash
$ pip install tox -r requirements-dev.txt
```


To run the test suite in your current python version:

```bash
$ py.test
```


To run the full test suite across all supported python versions:

```bash
$ tox
```
