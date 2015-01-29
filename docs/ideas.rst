Miscellaneous Ideas
===================


An Alternative Error Design
---------------------------

The error handling design approach used here of cascading function calls with
``&&`` and reporting the error state globally is not the only possible approach.
I have also spent some time on the complementary approach where each function
returns an error code with ``NULL`` representing success and the error code
returned as an opaque pointer type.  In this case function calls would be
cascaded with the much less ``?:`` familiar operator and there would be an
absolute obligation on the callers to deal with the results.  Perhaps something
along these lines::

    error_t error =
        do_this()  ?:
        do_that()  ?:
        do_the_other();
    if (error)
        report_error(error);

This approach could well have the advantage of providing much more control over
where and how errors are reported, but there are a number of difficulties:

* The ``?:`` operator is extremely unfamiliar, I don't believe I've ever used
  it!
* The opaque error codes would have to be dynamically allocated.
* Discarding error returns would result in memory leaks, this would be very easy
  to do by accident.  I think this is the main reason I've not taken this
  approach further.
* Annoyingly, the type name :type:`error_t` is already spoken for in
  ``<argp.h>``!
