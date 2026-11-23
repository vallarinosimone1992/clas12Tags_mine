#!/usr/bin/env perl
use strict;
use warnings;
use FindBin;
BEGIN { $ENV{GEMC} //= "$FindBin::Bin/../.."; }
use lib "$FindBin::Bin/../../api/perl";
use geometry;
use hit;
use bank;

# Full panel dimensions in mm; GEMC Box dimensions are half-lengths.
my ($panel_x, $panel_y, $panel_z) = (1000, 1000, 10);

chdir "$FindBin::Bin/../../experiments/mine" or die "Cannot enter experiments/mine: $!";
my %configuration = (
    detector_name => 'mine',
    factory       => 'TEXT',
    variation     => 'default',
    run_number    => 1,
    verbosity     => 0,
);

my %panel = init_det();
$panel{name}        = 'mine_panel';
$panel{mother}      = 'root';  # Generic GEMC world, configured in mine.gcard.
$panel{description} = 'MINE bootstrap scintillator panel';
$panel{pos}         = '0*mm 0*mm 0*mm';
$panel{type}        = 'Box';
$panel{dimensions}  = join ' ', map { ($_ / 2) . '*mm' } ($panel_x, $panel_y, $panel_z);
$panel{material}    = 'G4_PLASTIC_SC_VINYLTOLUENE';
$panel{color}       = '33aaff';
$panel{style}       = 1;
$panel{sensitivity} = 'mine';
$panel{hit_type}    = 'mine';
$panel{identifiers} = 'id manual 1';
my @variations = qw(default corner_fan triple_serpentine panel_double_serpentine parallel_fan_in);
for my $variation (@variations) {
    $configuration{variation} = $variation;
    print_det(\%configuration, \%panel);
}

my %hit = init_hit();
$hit{name}            = 'mine';
$hit{description}     = 'MINE plastic scintillator panel';
$hit{identifiers}     = 'id';
$hit{signalThreshold} = '0*MeV';
$hit{timeWindow}      = '100*ns';
$hit{prodThreshold}   = '1*mm';
$hit{maxStep}         = '1*mm';
$hit{riseTime}        = '1*ns';
$hit{fallTime}        = '5*ns';
$hit{mvToMeV}         = 1;
$hit{pedestal}        = 0;
$hit{delay}           = '0*ns';
for my $variation (@variations) {
    $configuration{variation} = $variation;
    print_hit(\%configuration, \%hit);
}

# MINE owns this bank. Field numbers are local to this bank, not CLAS12 IDs.
my @fields = (
    ['id', 1, 'Di', 'Panel identifier'],
    ['Edep', 2, 'Dd', 'Deposited energy (MeV)'],
    ['Evis', 3, 'Dd', 'Birks visible energy (MeV)'],
    ['Nphotons', 4, 'Dd', 'Expected scintillation photons'],
    ['Ncollected', 5, 'Dd', 'Expected collected photons'],
    ['Ntransported', 6, 'Dd', 'Expected photons at SiPM'],
    ['meanNpe', 7, 'Dd', 'Mean primary detected photoelectrons'],
    ['Npe', 8, 'Di', 'Sampled primary photoelectrons'],
    ['Nfired', 9, 'Di', 'Prompt fired pixels including dark and crosstalk'],
    ['Ndark', 10, 'Di', 'Prompt dark avalanches'],
    ['NcrossTalk', 11, 'Di', 'Prompt crosstalk avalanches'],
    ['Nafterpulse', 12, 'Di', 'Delayed afterpulses'],
    ['ADC', 13, 'Dd', 'Integrated waveform in provisional ADC units'],
    ['TDC', 14, 'Dd', 'Leading threshold crossing (ns); -1 if absent'],
    ['timingValid', 15, 'Di', 'Whether a leading threshold crossing exists'],
    ['t0', 16, 'Dd', 'Earliest depositing step time (ns)'],
    ['readoutDistance', 17, 'Dd', 'Produced-photon-weighted bulk capture distance (mm)'],
    ['fiberDistance', 18, 'Dd', 'Collected-photon-weighted route to selected end (mm)'],
    ['readoutMode', 19, 'Di', '0 reference; 1 WLS; 2 direct fiber'],
    ['unquenchedSteps', 20, 'Di', 'Charged depositing zero-length steps'],
    ['readoutEnd', 21, 'Di', 'Selected physical SiPM end (0 or 1)'],
    ['fiberModel', 22, 'Di', '0 Y-11; 1 YS-6; -1 clear'],
    ['fiberLayout', 23, 'Di', '0 anchor;1 fan;2 legacy double;3 corner;4 triple;5 panel double;6 straight;7 other table'],
    ['idealSiPM', 24, 'Di', 'Whether the ideal SiPM override is enabled'],
    ['effectivePDE', 25, 'Dd', 'Photon-weighted PDE before saturation'],
    ['Ncollected0', 26, 'Dd', 'Expected optically collected photons sensor 0'],
    ['Ntransported0', 27, 'Dd', 'Expected photons after coupling sensor 0'],
    ['meanNpe0', 28, 'Dd', 'Mean primary PE sensor 0'],
    ['Npe0', 29, 'Di', 'Sampled primary PE sensor 0'],
    ['Nfired0', 30, 'Di', 'Prompt fired pixels sensor 0'],
    ['Ndark0', 31, 'Di', 'Dark avalanches sensor 0'],
    ['NcrossTalk0', 32, 'Di', 'Crosstalk avalanches sensor 0'],
    ['Nafterpulse0', 33, 'Di', 'Afterpulses sensor 0'],
    ['ADC0', 34, 'Dd', 'Integrated waveform charge sensor 0'],
    ['TDC0', 35, 'Dd', 'Leading threshold crossing ns; -1 if invalid sensor 0'],
    ['timingValid0', 36, 'Di', 'Valid threshold crossing sensor 0'],
    ['effectivePDE0', 37, 'Dd', 'Photon weighted PDE sensor 0'],
    ['Ncollected1', 38, 'Dd', 'Expected optically collected photons sensor 1'],
    ['Ntransported1', 39, 'Dd', 'Expected photons after coupling sensor 1'],
    ['meanNpe1', 40, 'Dd', 'Mean primary PE sensor 1'],
    ['Npe1', 41, 'Di', 'Sampled primary PE sensor 1'],
    ['Nfired1', 42, 'Di', 'Prompt fired pixels sensor 1'],
    ['Ndark1', 43, 'Di', 'Dark avalanches sensor 1'],
    ['NcrossTalk1', 44, 'Di', 'Crosstalk avalanches sensor 1'],
    ['Nafterpulse1', 45, 'Di', 'Afterpulses sensor 1'],
    ['ADC1', 46, 'Dd', 'Integrated waveform charge sensor 1'],
    ['TDC1', 47, 'Dd', 'Leading threshold crossing ns; -1 if invalid sensor 1'],
    ['timingValid1', 48, 'Di', 'Valid threshold crossing sensor 1'],
    ['effectivePDE1', 49, 'Dd', 'Photon weighted PDE sensor 1'],
    ['opticalTable', 50, 'Di', '1 tabulated optics; 0 otherwise; see responseMode'],
    ['opticalHash', 51, 'Dd', 'Low 52 bits FNV1a of table or explicit optical config'],
    ['fiberCount', 52, 'Di', 'Number of fibers from table or explicit optical geometry; 0 legacy'],
    ['bothTimingValid', 53, 'Di', 'Both sensors have valid timing'],
    ['TDCmean', 54, 'Dd', 'Arithmetic mean of sensor TDCs ns; -1 if invalid'],
    ['TDCdifference', 55, 'Dd', 'TDC1 minus TDC0 ns; -1 if invalid'],
    ['responseMode', 56, 'Di', '0 legacy; 1 tabulated; 2 explicit Geant4 optical transport'],
    ['Nscintillation', 57, 'Dd', 'Actual Geant4 scintillation photon count; -1 in effective modes'],
    ['hitn', 99, 'Di', 'Hit number'],
);
insert_bank_variable(\%configuration, 'mine', 'bankid', 4600, 'Di', 'MINE response bank ID');
for my $field (@fields) {
    insert_bank_variable(\%configuration, 'mine', @$field);
}


# Keep the HIPO schema synchronized with this canonical bank definition.
my @hipo_entries = map {
    my ($name, $id, $type) = @$_;
    die "Unsupported MINE HIPO field type $type" unless $type eq 'Di' || $type eq 'Dd';
    $name . '/' . ($type eq 'Di' ? 'I' : 'D');
} @fields;
my $hipo_header = "$FindBin::Bin/../../source/hitprocess/mine/mine_hipo_schema.h";
open my $schema_file, '>', $hipo_header or die "Cannot write $hipo_header: $!";
print {$schema_file} "// Generated by geometry_source/mine/mine.pl; edit the canonical fields there.\n";
print {$schema_file} "#pragma once\nnamespace mine {\ninline const char* HipoDgtSchema() {\n    return \"";
print {$schema_file} join(',', @hipo_entries);
print {$schema_file} "\";\n}\n}\n";
close $schema_file or die "Cannot close $hipo_header: $!";
