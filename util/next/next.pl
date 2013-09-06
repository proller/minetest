#!/usr/bin/perl
use 5.16.0;
#use strict;
no strict qw(refs);
use warnings "NONFATAL" => "all";
no warnings qw(uninitialized);
no if $] >= 5.017011, warnings => 'experimental::smartmatch';
use utf8;
use lib::abs;

my $what = {
    minetest => qq{
proller:next_tools

proller:liquid63			882
proller:math				645
proller:sqlite
proller:json
proller:weather				892
proller:liquid_send			895
proller:liquid_default
proller:huge_cave
proller:weather_fog
proller:wingsuit
proller:collision
sapier:avoid_facedir_if_not_moving	879
sapier:modmgr_fixes			884
#ShadowNinja:bind_address		862 #crash on connect
proller:clouds				855
Zeg9:slippery				817
#Zeg9:wieldlight				816
MirceaKitsune:sun_moon_coloring		799
khonkhortisan:diagonal_rail		528
Novatux:forceload			606
Novatux:tool_callback			688
PilzAdam:vbo
sweetbomber:spawn			744
MirceaKitsune:brighter_sun_faces	772
sapier:fix_crash_on_lua_exception	890
sapier:lua_show_backtrace_on_error	891
sapier:fix_anaglyph_mode_selectionbox_not_beeing_honored	893
WilliamBundy:jump_fix			866
ShadowNinja:protection_support		856
sapier:fix_multiplayer_server_not_saved	846
},

    minetest_game => qq{
proller:sponge		185
proller:weather
proller:liquid63
#Zeg9:wieldlight		188
Jordach:moonflower	169
},

};

sub file_append(;$@) {
    my $n = shift;
    return unless open my $fh, '>>', $n;
    print $fh @_ ? @_ : $_;
    @_           ? @_ : $_;
}

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
my $report = [];
my $root   = lib::abs::path('.') . '/';
REPO: for my $repo ('minetest', 'minetest_game') {
    my $log = "$root$repo.log";
    unlink $log;
    my $pullroot = "https://github.com/minetest/$repo/pull/";
    my $dir      = $root . $repo . '_' . $target;
    sy "git clone https://github.com/proller/$repo.git $dir";
    chdir $dir;
    sy "
git reset --hard
git remote add upstream https://github.com/minetest/$repo.git
git fetch upstream
git checkout upstream/master
git branch -D $target
git checkout -b $target
";
    my $error;

    for my $from (split /\n+/, $what->{$repo}) {
        next if $from =~ /^(?:\s*#|$)/;
        $from =~ m{^\s*(?<user>\S+)[:/](?<branch>\S+)(\s+(?<pull>\S+))?(\s+(?<comment>.+))?};
        my $i = {%+, repo => $repo, pullfull => "$pullroot$+{pull}"};
        #$i->{branch} = $i->{user}, $i->{user} = undef if !$i->{branch};
        my $path = join '/', grep {$_} $i->{user}, $i->{branch};

        if ($i->{repo}) {
            sy "git remote add $i->{user} https://github.com/$i->{user}/$repo.git";
            sy "git fetch $i->{user}";
        }

        say "merging $path to $target";
        #if (local $_ = sy "git merge --no-edit $path") {
        if (local $_ = sy "git merge --no-edit -s recursive -X patience $path") {
            push @$report, {%$i, status => 'fail', code => $_};
            file_append $log, "\n\n\n\n!!!!!!!!!!!!!!!!!\n Merge $path to $target failed:";
            sy "git status >> $log";
            sy "git diff >> $log";
            ++$error, last REPO if 'fail' ~~ @ARGV;
            sy "git reset --hard";
        } else {
            push @$report, {%$i, status => 'ok'};
        }

    }
    my $diff = qx{git diff --stat origin/next};
    unless ($diff) {
        say "no changes";
        goto UP;
    }

    say "changed $diff";
    my $test = sy "cmake . -DRUN_IN_PLACE=1 && make -j4" if $repo eq 'minetest' and !$error and !('notest' ~~ @ARGV);
    say "test = [$test]";
    sy "git push -f" if !$test and !('nopush' ~~ @ARGV);
  UP:
    chdir '..';
    sy qq{git commit -m "merge log" $log};
}

unlink $root . 'report.log';
for my $r (@$report) {
    local $_ = join "\t", $r->{status}, "$r->{repo} $r->{user}:$r->{branch}", $r->{code}, ($r->{pull} ? $r->{pullfull} : ()), "\n";
    print;
    file_append $root . 'report.log', $_;
}
sy qq{git commit -m "report log" ${root}report.log};
sy "git push" if !('nopush' ~~ @ARGV);
