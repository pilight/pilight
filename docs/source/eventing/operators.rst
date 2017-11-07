Operators
=========

.. rubric:: Arithmetic

+--------------+-------------------------------------------------+--------------------------+
| **Operator** | **Description**                                 | **Example**              |
+--------------+-------------------------------------------------+--------------------------+
| \+           | Adds two operands.                              | 10 + 20 = 30             |
+--------------+-------------------------------------------------+--------------------------+
| −            | Subtracts second operand from the first.        | 10 − 20 = -10            |
+--------------+-------------------------------------------------+--------------------------+
| \*           | Multiplies both operands.                       | 10 * 20 = 200            |
+--------------+-------------------------------------------------+--------------------------+
| /            | Divides numerator by denominator.               | | 10 / 20 = 2            |
|              |                                                 | | 10 / 3 = 3.333...      |
+--------------+-------------------------------------------------+--------------------------+
| \\ [#f1]_    | Integer division for first operand by second    | 10 \\ 3 = 3              |
+--------------+-------------------------------------------------+--------------------------+
| %            | Remainder after an integer division.            | | 20 % 10 = 0            |
|              |                                                 | | 11 % 3 = 2             |
+--------------+-------------------------------------------------+--------------------------+

.. [#f1] The backslash is also used as an ``escape`` character. So if you want to use the integer division operator in a rule you must write your rule like this:

         .. code-block:: json

            {
               "rules": {
                  "rule": "IF 3 \\ 2 = 1 THEN switch DEVICE light TO on",
                  "active": 1
               }
            }

.. rubric:: Relational

+--------------+-------------------------------------------------+--------------------------+
| **Operator** | **Description**                                 | **Example**              |
+--------------+-------------------------------------------------+--------------------------+
| ==           | Checks if the first and second (numeric)        | | 20 == 10 = false       |
|              | operand are equal.                              | | 10 == 10 = true        |
+--------------+-------------------------------------------------+--------------------------+
| !=           | Checks if the first and second (numeric)        | | 20 != 10 = true        |
|              | operand are unequal.                            | | 10 != 10 = false       |
+--------------+-------------------------------------------------+--------------------------+
| IS           | Checks if the first and second (character)      | | a IS b = false         |
|              | operand are equal.                              | | a IS a = true          |
+--------------+-------------------------------------------------+--------------------------+
| >=           | Checks if the first operand is greater than or  | | 20 >= 10 = true        |
|              | equal to the second operand.                    | | 10 >= 20 = false       |
|              |                                                 | | 10 >= 10 = true        |
+--------------+-------------------------------------------------+--------------------------+
| >            | Checks if the first operand is greater than     | | 20 > 10 = true         |
|              | the second operand.                             | | 10 > 20 = false        |
|              |                                                 | | 10 > 10 = false        |
+--------------+-------------------------------------------------+--------------------------+
| <=           | Checks if the first operand is less than or     | | 20 <= 10 = false       |
|              | equal to the second operand.                    | | 10 <= 20 = true        |
|              |                                                 | | 10 <= 10 = true        |
+--------------+-------------------------------------------------+--------------------------+
| <            | Checks if the first operand is less than        | | 20 < 10 = false        |
|              | the second operand.                             | | 10 < 20 = true         |
|              |                                                 | | 10 < 10 = false        |
+--------------+-------------------------------------------------+--------------------------+

.. versionadded:: 8.0 ISNOT operator added

+--------------+-------------------------------------------------+--------------------------+
| **Operator** | **Description**                                 | **Example**              |
+--------------+-------------------------------------------------+--------------------------+
| ISNOT        | Checks if the first and second (character)      | | a ISNOT b = true       |
|              | operand are not equal.                          | | a ISNOT a = false      |
+--------------+-------------------------------------------------+--------------------------+

.. rubric:: Logical

+--------------+-------------------------------------------------+--------------------------+
| **Operator** | **Description**                                 | **Example**              |
+--------------+-------------------------------------------------+--------------------------+
| AND          | Checks if the first and second operands are     | | 1 AND 1 = true         |
|              | true.                                           | | 1 AND 0 = false        |
|              | | - Numeric values other than 0 are true        | | foo AND false = true   |
|              | | - Strings equal to ``0`` are false            | | true AND true = true   |
|              | | - Strings other than ``0`` are true           | | false AND false = true |
+--------------+-------------------------------------------------+--------------------------+
| OR           | Checks if the first or second operands are      | | 0 OR 0 = false         |
|              | true.                                           | | 1 OR 0 = true          |
|              | | - Numeric values other than 0 are true        | | foo OR false = true    |
|              | | - Strings equal to ``0`` are false            | | true OR true = true    |
|              | | - Strings other than ``0`` are true           | | false OR false = true  |
+--------------+-------------------------------------------------+--------------------------+
