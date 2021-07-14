clc
clear all

N = 8 ;                     % Number of bits
resolution = 2*pi/2^N ;     % Step size 
t = 0:resolution:2*pi-resolution ; % time
taps = (sin(t)+1)/2*(2^N-1) ;
taps = uint8(taps);
plot(taps, '-r', 'linewidth', 1.5)
grid on
xlim([0, length(taps)])
ylim([0,max(taps)])

taps = dec2hex(taps);

% Array ready to copy and paste
fprintf('{')
for i = 1:length(taps)
    fprintf('0x')
    fprintf(taps(i,:))
    if i ~= length(taps)
        fprintf(', ')
    else
        fprintf('};\n')
    end
end