#!/usr/bin/perl
use 5.16.0;

sub sy (@) {
    #warn @_;
    system @_;
    if ($? == -1) {
        #print "failed to execute: $!\n";
        return $?;
    } elsif ($? & 127) {
        #printf "child died with signal %d, %s coredump\n",($? & 127),  ($? & 128) ? 'with' : 'without';
        return $?;
    } else {
        printf "child [@_] exited with value %d\n", $? >> 8 if $?;
        return $? >> 8;
    }
}
my $target = 'next';
my $pullroot = 'https://github.com/minetest/minetest/pull/';
sy "git clone https://github.com/proller/minetest.git next";
#sy "mkdir -p next";
chdir 'next';
sy "
git reset --hard
git remote add upstream https://github.com/minetest/minetest.git
git fetch upstream
git checkout upstream/master
git branch -D $target
git checkout -b $target
";
#git pull --all
my $report = [];
for my $from (split /\n+/, qq{
proller:next_tools
proller:liquid63
proller:math
proller:sqlite
proller:json
proller:heat
proller:weather
sapier:avoid_facedir_if_not_moving
sapier:modmgr_fixes
ShadowNinja:bind_address
proller:clouds
Zeg9:slippery
Zeg9:wieldlight
MirceaKitsune:sun_moon_coloring
khonkhortisan:diagonal_rail
Novatux:master
PilzAdam:vbo
sapier:pathfinder_bugfixes
sweetbomber:spawn			744
MirceaKitsune:brighter_sun_faces	772
sapier:fix_crash_on_lua_exception	890
sapier:lua_show_backtrace_on_error	891
BlockMen:hotbar 			849
}
  )
{
    next if $from =~ /^(?:\s*#|$)/;
    $from =~ m{^\s*(?<repo>\S+)[:/](?<branch>\S+)(\s+(?<pull>\S+))?};
    my $i = {%+};
    #$i->{branch} = $i->{repo}, $i->{repo} = undef if !$i->{branch};
    my $path = join '/', grep {$_} $i->{repo}, $i->{branch};

    if ($i->{repo}) {
        sy "git remote add $i->{repo} https://github.com/$i->{repo}/minetest.git";
        sy "git fetch $i->{repo}";
    }

    say "merging $path to $target";
    if (local $_ = sy "git merge --no-edit $path") {
        push @$report, {%$i, status => 'fail', code => $_};
        sy "git reset --hard";
        #last;
    } else {

    push @$report, {%$i, status => 'ok'};
}

}
for my $r (@$report) {
    say "$r->{status}\t$r->{repo}:$r->{branch}\t$r->{code}\t$pullroot$r->{pull}";
}
