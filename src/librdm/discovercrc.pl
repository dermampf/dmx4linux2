#!/usr/bin/perl

$crc = 0;
for ($i = 0; $i < scalar(@ARGV); $i++)
{
    my $value = hex($ARGV[$i]);

    printf $value . "\n";
    $crc += $value;
}

printf "CRC:%04X\n", $crc;

printf "CRC:%02X %02X %02X %02X\n", ($crc>>8)|0xAA, ($crc>>8)|0x55, ($crc&0xFF)|0xAA, ($crc&0xFF)|0x55;
