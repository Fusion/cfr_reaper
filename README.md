This is a very light program that can be run as process #1 in a container such as Docker.


# "What do I need this for?" you may ask.

This is quite simple, really. Docker, while being a great tool, suffers from a fairly 
annoying limitation: if used as prescribed by its authors, you may end up with a lot 
of "zombie" processes. This is because it is recommended to run Docker to execute 
only a single program/daemon. For instande, run `nginx` or `jenkins` or even more 
innocuous `tail` or what not. As users, we always find creative ways to shoot 
ourselves in the foot.

Lets's take an example:

    docker run -p 8080:8080 jenkins

This will run ok until we start exec'ing a side shell and installing more daemons, 
that we wish to run alongiside jenkins. Through the magic of daemonization, 
these guys will end up being children of process #1. In this case: jenkins.

Now, jenkins can perfectly well handle its own children lifecycle, but where do 
these ones come from? Unlike the classic `init` process, it is unfair to expect 
of specialized software to behave like Unix's top cleaner. And that is how you end 
up with zombies processes with no one around to wait on them.

The side efffects can be spectacular, too. From running out of entries in the 
system's process table, to not closing file system level locks.


# "So, how do I use this? I am already running `docker run tail -f /var/log/messages`"

Are you? Oh, well, I've done worse.

Here is how to keep running your container without too much aggravation. 
These steps have to be taken inside your container:

1. Rename `tail` to something else, say `actualtail`
2. Rename `cfr_reaper` to `tail`
3. Upon start up, `cfr_reaper/tail` will be invoked
4. It may also be invoked later from the command line

Regarding #3:

When you start your container, the program will detect that it is running 
as `init` and will install itself as a daemon then execute the actual commmand. 
The arguments passed to `tail` will be passed to `actualtail` or, alternatively, 
you can specify arguments, one per line, in a file called `cfr_reaper_args` found 
either in the current directory or under `/etc`

How will the program know where to find the actual command?

The command name will be in a file called `cfr_reaper_actual` found, again, either in 
the current directory or under `/etc`

Regarding #4:

Later on, when typing `tail <something>` you will still be invoking `cfr_reaper` 
but it will detect that it is already running as a daemon and simply 
execute the actual command.


# How do I test this? How do I create a zombie?

Have a look at `mk_zombie.c` -- it is very simple.


# What if I have more questions?

Feel free to contact me. I cannot guarantee that my explanations will be less 
confusing than the content of this file, though.

-Chris.
