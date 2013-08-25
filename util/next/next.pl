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
my $report = [];
#branch pull
for my $from (split /\n+/, qq{
proller:next_tools
proller:liquid63			882
proller:math				645
proller:sqlite
proller:json
proller:heat				883
proller:weather				892
sapier:avoid_facedir_if_not_moving	879
sapier:modmgr_fixes			884
ShadowNinja:bind_address		862
proller:clouds				855
Zeg9:slippery				817
Zeg9:wieldlight				816
MirceaKitsune:sun_moon_coloring		799
khonkhortisan:diagonal_rail		528
Novatux:master				606
PilzAdam:vbo
sapier:pathfinder_bugfixes		887
sweetbomber:spawn			744
MirceaKitsune:brighter_sun_faces	772
sapier:fix_crash_on_lua_exception	890
sapier:lua_show_backtrace_on_error	891
BlockMen:hotbar				849
sapier:fix_anaglyph_mode_selectionbox_not_beeing_honored	893
WilliamBundy:jump_fix			866
ShadowNinja:protection_support		856
sapier:fix_multiplayer_server_not_saved	846
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
sy "cmake . && make -j4 && git push -f";
for my $r (@$report) {
    say join "\t",$r->{status},"$r->{repo}:$r->{branch}",$r->{code}, ($r->{pull} ? "$pullroot$r->{pull}" : ());
}
