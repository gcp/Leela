About
=====

This is the original source of the Go/Baduk program 'Leela'.

I had an interest in computer go for a long time, but existing approaches were based on pattern matching and extensive manual tuning, which did not appeal to me very much. Things were turned on their head though, when in 2006 Rémi Coulom published _Efficient Selectivity and Backup Operators in Monte-Carlo Tree Search_ and (re-)invented MCTS.
Suddenly, we had an algorithm that scaled with computing power and thinking time, and that turned the existing computer-go world upside down, making me interested in entering that competition.
It also marked the beginning of the end for humanity, but that would take a few more years to play out.

Leela was started around 2006-2007 and incorporated both public ideas published in those years (UCT, RAVE, Elo-rating move patterns, ...) and some additional tweaks. In 2008 it entered the Computer Olympiad, and did rather well on very modest hardare with Bronze and Silver medals. But lacking a clear path forward what to do with a Go engine, I lost interest for several years.

In 2014 two researchers from the University of Edinburgh published a paper _Teaching Deep Convolutional Neural Networks to Play Go_, showing that a DCNN could play Go well enough to beat GNU Go, even without searching. Mere days later (showing that multiple teams had been doing similar research), DeepMind published their own paper _Move Evaluation In Go Using Deep Convolutional Neural Networks_, having thrown more hardware at the problem and getting even better results. Adding a second neural network to aid positional evaluation eventually culminated in the Alpha Go effort. The new interest in computer Go also rekindled mine and I cleaned up Leela and made it freely available. After reading the relevant research papers, I decided to add a DCNN to Leela as well, to learn about neural networks and GPU programming. Leela received several updates, and also large improvements to its classical Monte Carlo evaluation.

After the release of Alpha Go Zero and _Mastering the game of Go without human knowledge_, and some maths on the computing power required to duplicate that effort,
I decided to start an open project to replicate the results - and get them available to the public - named Leela Zero. This was based off of the code of Leela. The origins are in the "zero" branch in this repository, and should match the first commits of the [leela-zero project on Github](https://github.com/leela-zero/leela-zero). I hope. Approximately.

Status
======

While I still get requests from time to time about the original Leela, I've not had the time nor energy to go back and update it with the new advances made, or retrain it with the (much better) data that is now available from Leela Zero. Yet, the ease of installation and basic GUI are still liked by many people. Because the engine's network is trained from human games, it also plays a bit more human like. At least the way humans played until they started learning from Leela Zero and friends.

The source code of the graphical interface is in [another repository here on Github](https://github.com/gcp/LeelaGUI).

As this branch of research died out pretty quickly when the neural network approach appeared, the Monte Carlo evaluation code may be of some historical interest. It is stronger than Pachi, which probably was the strongest free (non neural network) MCTS
engine around ~2017. It uses a combination of Policy Gradient Reinforcement Learning,
combined with Simulation Balancing and strong regularization, to tune a large pattern
database from self-play. Memory requirements and performance are maintained by using
Minimal Perfect Hashing to store the pattern database, and cache locality is improved by using a "bad" hash that maximizes the collisions per bucket.

Building
========

The engine needs boost and OpenCL libraries and development headers (or OpenBLAS) to compile. The included Makefile likely needs some modification to work on random Linux systems. Likewise the Visual Studio projects will need their include and lib directories modified.

For the dependencies to build the GUI, see that repository.

Contributing
============

I do not accept contributions to or bug reports about this code. Maintaining an open source effort is a lot of work, and I do not have the time and energy to maintain the project at this point in time. If you wish to improve it, feel free to fork the repostiory and make your fork the best one there is. But do me **ONE** favor, and do keep Leela's name in your fork...

Thanks
======

I want to thank everyone who contributed to the development of Leela: other authors in the computer-go community that provided data or experimental results on the computer-go list, the people that bought a copy from me early on, Nick Wedd for organizing the KGS tournaments, Don Dailey for many public discussions and MCTS scaling research, IRC friends that helped out in various places, and everyone who jumped onto and contributed to the Leela Zero project.

License
=======

The code is licensed under the MIT license.
