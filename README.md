Written by HC Lee

* Code convention
Please follow the below code conventions.

1) tabstop = 2
2) don't exceed 80 columns
   (in vim, :set colorcolumn=80 would be useful.)
3) comment convention
  3-1) multi-line comments
       /**
        *
        ..
        */

  3-2) one line comment
      // ...

  3-3) one line comment with not-comment code
      int a; /* like this */

  3-4) function declaration
      /**
       * description
       * @a: blahblah..
       * @b: blahblah..
       */
      void blah(int a, int b)

4) Please remove tail empty characters
  e.g. int a;    <-- those three blanks.

5) Camel-case naming
