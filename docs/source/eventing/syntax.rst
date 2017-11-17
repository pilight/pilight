.. role:: event-success
.. role:: event-fail

Syntax
======

- `Introduction`_
- `Rule Parsing`_
   - `Basic Structure`_
   - `True or False`_
   - `AND and OR`_
   - `Hooks`_
- `Operator`_
- `Devices`_
- `Functions`_
- `Actions`_
- `Using Device Parameters`_
- `Optimizations`_

Introduction
------------

The pilight eventing library allows you to automate your house by writing event rules. These rules tell pilight what should happen at a given time or at a given event. pilight event rules follow the regular mathematical conventions and because you can work with an infinite number of hooks, they can become highly complex but are also highly flexible.

As with everything inside pilight, the eventing library is also highly modular. This means that the operators, actions, and functions are external modules that can be easily extended. Developers and users do not have to touch the core of the eventing library to add additional features.

Rule Parsing
------------

There are few things that are useful to know before working with pilight event rules. When you better understand how pilight parses the rules, you can write really efficient and advanced rules.

.. _Basic Structure:
.. rubric:: Basic Structure

All rules basically consist of the same structure:

.. code-block:: guess

   IF ... THEN ...

A rule always start with the IF, followed by the rule logic, and is closed by a THEN. All actions are put after the THEN.

.. _True or False:
.. rubric:: True or False

Let us start with how pilight handles succeeding or failing conditions. pilight converts each logical mathematical and logical step into a true or false. The final condition must be true before triggering an action. If the final result is false, the action is skipped. These two examples will both succeed:

:event-success:`Succeeds`

.. code-block:: guess

   IF (1 == 1) == True THEN

:event-success:`Succeeds`

.. code-block:: guess

   IF (1 == 1) != False THEN

These two examples will both fail:

:event-fail:`Fails`

.. code-block:: guess

   IF (1 == 0) IS True THEN

:event-fail:`Fails`

.. code-block:: guess

   IF (1 != 1) IS False THEN

.. _AND and OR:
.. rubric:: AND and OR

In the first examples we only used one condition but this does not allow us to make very advanced and flexible rules. Therefore we need to be able to combine several conditions in a single rule. This is possible by combining them with AND and OR operators. Let us take a look at some basic examples:

:event-fail:`Fails`

.. code-block:: guess

   IF 1 == 1 AND 1 == 2 THEN ...

:event-success:`Succeeds`

.. code-block:: guess

   IF 1 == 1 OR 1 == 2 THEN ...

:event-fail:`Fails`

.. code-block:: guess

   IF 1 == 2 OR 1 == 1 AND 2 == 3 THEN ...

:event-success:`Succeeds`

.. code-block:: guess

   IF 1 == 1 OR 1 == 1 AND 2 == 3 THEN ...

:event-fail:`Fails`

.. code-block:: guess

   IF 1 == 2 AND 1 == 1 OR 2 == 3 THEN ...

:event-success:`Succeeds`

.. code-block:: guess

   IF 1 == 1 AND 2 == 2 OR 2 == 3 THEN ...

In the first two examples only a single AND and OR operator is used. In the rest of the examples we see multiple AND and OR operators. It is good to know that pilight evaluates these rules from left to right and only parses as much as necessary. In these examples only the green parts are actually evaluated and the red parts are skipped. In rule one we use an AND operator. This means that both conditions have to succeed for the rule to succeed, therefore, pilight has to evaluate both conditions. In the second rule we use the OR operator. This means that either one of the two conditions has to succeed for the whole rule to succeed. In this case, the first condition already succeeded so pilight knows it does not have to parse anything else and skips right to the action. The same logic as in rule two can be seen in rule four. This rule starts with an OR statement that already succeeds so the subsequent AND conditions do not have to be evaluated. In rule five we see that pilight parses the first condition which fails and because this condition was part of an AND operator we don't have to evaluate the second condition. pilight does evaluate the last condition because it can change the final outcome of rule five. The same happens in rule six. The first two conditions as part of the AND operator succeed so we don't have to evaluate the last condition.

.. _Hooks:
.. rubric:: Hooks

As we saw in our first examples, hooks can be used inside pilight rules. This can be useful to better structure and combine the various conditions of our rules. Let us create even more complex rules and see how hooks can change the outcome of a rule without changing the conditions.

:event-success:`Succeeds`

.. code-block:: guess

   IF 1 == 2 OR 2 == 3 AND 2 == 3 OR 1 == 1 THEN ...

:event-fail:`Fails`

.. code-block:: guess

   IF (1 == 2 OR 2 == 3) AND (2 == 3 OR 1 == 1) THEN ...

Operator
--------

Various mathematical operators can be used to do calculations inside our rules. A list of these operators can be found further on in this manual. Let us just show some basic self-explanatory examples:

:event-success:`Succeeds`

.. code-block:: guess

   IF 1 + 1 == 2 THEN ...

:event-success:`Succeeds`

.. code-block:: guess

   IF 2 % 2 == 0 THEN ...

:event-success:`Succeeds`

.. code-block:: guess

   IF 1 < 10 THEN ...

:event-fail:`Fails`

.. code-block:: guess

   IF 10 / 2 == 4 THEN ...

Devices
-------

pilight rules are quite useless if we cannot work with live data. This live data comes from our devices in and around the house. So let us say we have a switch called switch and we use this configured device to create a rule like this:

.. code-block:: guess

   IF switch.state IS on THEN ...

Depending on the actual state of the switch this rule will succeed or fail. Let us now use a dimmer device called *dimmer*.

.. code-block:: guess

   IF dimmer.dimlevel > 10 THEN ...

Again, this rule will succeed or fail depending on the actual dimlevel of the configured dimmer device. These two examples can of course be combined:

.. code-block:: guess

   IF switch.state IS on AND dimmer.dimlevel > 10 THEN ...

As you can also see, the fields (*state* or *dimlevel*) we can use depends on the device we are using inside our rules. A switch does not have a *dimlevel* field but a dimmer does have a *state* field.

.. versionadded:: 8.0 rules based on received codes

Some devices are only used inside rules. Configuring them as explicit devices might sometimes feel a bit bloated. Therefor, pilight allows you to trigger rules based on received codes instead of device updates:

.. code-block:: guess

   IF archtech_switch.state IS on AND archtech_switch.id == 123456 AND arctech_switch.unit == 0 THEN ...

In this case, an action will be triggered as soon as an ``archtech_switch`` code is received with a specific state, id, and unitcode. The ``arctech_switch`` doesn't have to be configured as an explicit device for this rule to work.

Functions
---------

In some cases, standard operators limit us in writing our rules. For example, calculating with time is a hideous task considering that hours do not go above 24, minute and seconds do not go above 60, and there are no negative numbers. Other functionality like randomization are also not possible in the standard event operators. This more advanced functionality is added in the form of function. A simple example:

.. code-block:: guess

   IF datetime.hour == RANDOM(21, 23) THEN ...

As we can see in this example we use the RANDOM function to check if the hour is either 21, 22, or 23. This allows us to trigger an action on random hours each day. Actions can also be nested for more advanced logic:

.. code-block:: guess

   IF datetime.hour == RANDOM(RANDOM(21, 22), RANDOM(22, 23)) THEN ...

The output of this RANDOM function is the same as with the previous example, but the idea should be clear.

Actions
-------

Actions are the final goal of our rules. These actions tell pilight what should happen when certain conditions have been met. A rule can contain unlimited number of actions and each action can trigger an unlimited number of devices. First two examples of basic actions triggering a switch called *lamp* and a dimmer called *ambientLight*:

.. code-block:: guess

   IF ... THEN switch DEVICE lamp TO on
   IF ... THEN dim DEVICE ambientLight TO 10

Both actions only trigger a single device. However, if we wanted to trigger both device to just on we can combine them in a single action:

.. code-block:: guess

   IF ... THEN switch DEVICE lamp AND ambientLight TO on

As we can see here, the switch action takes at least the DEVICE and TO parameters. In case of the switch action, several values (as in devices) can be combined by separating them with ANDs. We can also combine dim and switch action would we want to switch the *lamp* to on and dim the *ambientLight* to dimlevel 10 based on the same condition:

.. code-block:: guess

   IF ... THEN switch DEVICE lamp TO on AND dim DEVICE ambientLight TO 10

We can combine an unlimited number of actions like this. Again we see that we use the AND to combine several actions. We can also switch several devices across several actions in a single rule. Let's say we have a relay connected to our television set called television that we want to turn on as well.

.. code-block:: guess

   IF ... THEN switch DEVICE lamp AND television TO on AND dim DEVICE ambientLight TO 10

Using Device Parameters
-----------------------

Device parameters can be used as rule input almost everywhere. Let us look at a few examples to demonstrate this:

.. code-block:: guess

   IF 1 == 1 THEN dim DEVICE dimmer TO dimmerMax.dimlevel FOR dimmerDuration.dimlevel

In this case we use three dimmer devices. One dimmer called dimmer that we actually want to dim, and two dimmers that changes the way this rule behaves. The dimmerMax device tells pilight to what value the dimmer should dim. The dimmerDuration device tells pilight how long it should take to reach that dimlevel. Another example:

.. code-block:: guess

   IF 1 == 1 THEN switch DEVICE lamp1 TO lamp2.state

In this case we want to switch the device lamp1 to the same state as the device lamp2.

Device parameters can also be used in function:

.. code-block:: guess

   IF RANDOM(randomLow.dimlevel, randomHigh.dimlevel) == 10 THEN switch DEVICE lamp1 TO on

In this case we use two dimmers called randomLow and randomHigh to dynamically change the input of the RANDOM function used in this rule. A comprehensive and advanced example:

.. code-block:: guess

   IF sunriseset.sunset == DATE_FORMAT(DATE_ADD(datetime, +1 HOUR), \"%Y-%m-%d %H:%M:%S\", %H.%M) THEN switch DEVICE lamp1 TO on

Optimizations
-------------

Although pilight is extremely fast in evaluating event rules, simple steps can be made to further improve the performance of your events. A rule is actually nothing more then several evaluations of conditions. Let's see how we can optimize time based events. E.g.

.. code-block:: guess

   IF datetime.hour == 23 AND datetime.minute == 0 AND datetime.second == 0 THEN ...

To optimize this rule, we need to check how many times a specific condition is evaluated. Rules containing datetime values will be evaluated each second. So each part has a certain chance to be true each day based on an evaluation every second:

Hour is 23 3600 times a day (60 minutes * 60 seconds).
Minute is 0 1440 times a day (24 hours * 60 seconds).
Second is 0 1440 times a day (24 hours * 60 minutes).

So in the above rule, the first step evaluates true 3600 times, the second step 1 times, and the last step also 1 time. Let us see what happens when we change the rule:

.. code-block:: guess

   IF datetime.second == 0 AND datetime.minute == 0 AND datetime.hour == 23 THEN ...

In this case, the first step is evaluates true 1440 times, the second step 1 times, and the last step also 1 time. So we can conclude that the first rule triggers 3602 evaluations and the last rule only 1442. So by simply shifting the conditions, we can increase the performance of a single rule.

Let's take another example. In this case we trigger an event based on the sunset time. Let's use the sunset time of 19:00 and assume we turn off the Christmas tree at 0:00 each day.

.. code-block:: guess

   IF ((sunriseset.sunset == (datetime.hour + (datetime.minute / 100)) AND christmasstree.state IS off) AND datetime.second == 0) THEN switch DEVICE christmasstree TO on

Again, the time that each evaluation is true each day based on an evaluation each second:

Sunset is 19:00 60 times a day (60 seconds).
Hour is 19 3600 times a day (60 minutes * 60 seconds).
Minute is 0 1440 times a day (24 hours * 60 seconds).
Second is 0 1440 times a day (24 hours * 60 minutes).
State is off for 68400 times a day (19 hours * 60 minutes * 60 seconds).

So in the above rule, the first step evaluates true 60 times, the second step 60 times, and the last step only 1 time. Although this rule uses not much evaluations for it to trigger, the first evaluation does take several math formulas to solve. Let's again see what happens when we change the rule:

.. code-block:: guess

   IF ((datetime.second == 0 AND sunriseset.sunset == (datetime.hour + (datetime.minute / 100))) AND christmasstree.state IS off) THEN switch DEVICE christmasstreeTO on

In this case, the first evaluation is true 1440 times, the second part 60 times, and the last part only 1 time. So, moving the seconds to the beginning made the rule worse. The initial rule only needed 121 evaluates to be true, the second rule need 1501 evaluations. So, although the first rule had maths in the first evaluation, we could reduce the overall evaluations needed by 14 times by doing this.

If you want to improve pilight performance, please take a close look at how your conditions are ordered in each rule. Shifting them around can increase the evaluation performance.

In this case, we use the datetime device called datetime in the DATE_ADD function and the DATE_ADD function is subsequently used as input for the DATE_FORMAT function. These example should give you an idea about how we can use device parameters to dynamically change the behaviour of our rules.