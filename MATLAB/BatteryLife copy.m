%% =====================================================================
%  BatteryLife.m — Power budget for the smart trash bin
%  Course: 34315 IoT application and infrastructure implementation F26
%
%  Beregner gennemsnitlig strømtræk og batterilevetid for skraldespanden
%  baseret på state machine duty cycle.  Sammenligner to batteri-setups:
%
%    1)  9V Alkaline      (nominel 9.0V, ~550 mAh)
%    2)  2× 18650 Li-ion  (nominel 7.2V, ~3000 mAh pr. celle)
%
%  18650-setup'et KRÆVER en 2S BMS (Battery Management System) for at
%  håndtere over-/under-spænding, over-strøm og kortslutning.  Uden BMS
%  kan Li-ion celler over-aflades og dermed beskadiges (eller bryde i
%  brand!).  Quiescent-strømmen i en typisk 2S BMS (~50 µA) er
%  inkluderet i beregningen.
%
%  Buck:  MP1584EN  (Vin: 4.5–28V, low quiescent ~0.1 mA, ~88-92% η)
%
%  Power flow:   Battery --[BMS]--[BUCK]--> 5V rail --[NodeMCU LDO]--> 3.3V
%  5V powers:    HC-SR04, LM35, ULN2003 + 28BYJ-48
%  3.3V powers:  ESP8266 core, SH1106 OLED
%
%  REGN UNDER ANTAGELSE:  skraldespanden gennemgår alle states 1× / døgn,
%                         HC-SR04 pinges hvert 15. minut i CHECK state
% =====================================================================

clear; clc; close all;

%% ---------- 1. JUSTERBARE PARAMETRE (ret efter målinger) -------------

% --- Buck converter MP1584EN (tilret når prototypen er målt) ---
eta_buck      = 0.90;       % MP1584 typ. 88-92% ved lav last
                            % (bedre end LM2596 fordi switching freq er højere)
I_buck_quiet  = 0.1e-3;     % A — MP1584 quiescent ~0.1 mA
                            %     (LM2596 ville være ~5 mA — meget værre)
V_rail        = 5.0;        % buck output rail

% --- Batterier ---
% Hver batteri-konfiguration har:
%   cap_mAh : kapacitet pr. pakke (mAh ved typisk last)
%   V_nom   : nominel pakke-spænding
%   I_BMS   : ekstra quiescent fra BMS (kun relevant for Li-ion)
%   label   : display navn

batt.Alkaline_9V.cap_mAh = 550;            % Duracell / Energizer 9V alkaline
batt.Alkaline_9V.V_nom   = 9.0;
batt.Alkaline_9V.I_BMS   = 0;              % ingen BMS nødvendig
batt.Alkaline_9V.label   = '9V Alkaline';

batt.Li18650.cap_mAh = 7000;               % 2x 3500 mAh
batt.Li18650.V_nom   = 7.2;                % 2× 3.6V i serie
batt.Li18650.I_BMS   = 50e-6;              % 2S BMS quiescent ~50 µA (typ.)
batt.Li18650.label   = '2\times 18650 Li-ion';

% --- Hvor lang tid tilbringes pr. døgn i hver state (sek) ---
%     Antagelse:  poserne fyldes løbende henover dagen, lukkes om aftenen,
%                 tømmes næste morgen.  Justér hvis brugsmønstret er andet.
T_day = 24*3600;            % sekunder pr. døgn
t.LOAD       = 60;          % 1 m  fra man har tømt til man har loadet en ny pose
t.CHECK      = 22 * 3600;   % 16 t poseren fyldes gennem dagen
t.EMPTY_ME   = 2  * 3557;   % ca. 2 t  venter på at blive tømt (Har trukket lidt fra for ikke at gå over 24h samlet)
t.STEP_OPEN  = 2;           % 2 sek stepper når LOAD->CHECK
t.STEP_CLOSE = 12;          % 12 sek stepper i CLOSE state
t.STEP_RESET = 12;          % 12 sek stepper når EMPTY_ME->LOAD
% sanity check
assert( abs( (t.LOAD+t.CHECK+t.EMPTY_ME + t.STEP_OPEN+t.STEP_CLOSE+t.STEP_RESET) - T_day ) < 30, ...
        'State-tider summerer ikke til 24h');

%% ---------- 2. KOMPONENT STRØMTRÆK (typiske værdier) ------------------
% Alle strømme refereret til komponentens egen forsyningsspænding.
% NodeMCU LDO (AMS1117) er pass-through: I_in_5V ≈ I_out_3.3V.

% ESP8266 NodeMCU (WiFi associated, ingen sleep)
I_esp_3v3      = 75e-3;     % A — Espressif typ. ~70-80 mA WiFi connected idle
                            %     peaks ~170 mA under TX (gns. 75 mA inkl. throttle)

% SH1106 128x64 OLED display (3.3V, ca. 30% pixels lit)
I_oled_3v3     = 10.1e-3;     % A — typ. 8-15 mA, vælger 12 mA gennemsnit

% LM35 temperatursensor (5V)
I_lm35_5v      = 60e-6;     % A — datasheet: <60 µA quiescent

% HC-SR04 ultralyd (5V)
I_hcsr04_idle  = 2e-3;      % A — quiescent
I_hcsr04_act   = 15e-3;     % A — under 10 ms ranging burst
% Sensoren pinges nu hvert 15. minut i CHECK state (forrige version: 5 s)
T_check_poll   = 15 * 60;   % sek mellem ultralyds-målinger
duty_hcsr04    = 0.010 / T_check_poll;
I_hcsr04_avg   = I_hcsr04_idle + duty_hcsr04 * (I_hcsr04_act - I_hcsr04_idle);

% ULN2003 + 28BYJ-48 stepper (5V)
I_uln_idle     = 0.5e-3;    % A — driver quiescent (alle inputs LOW, coils off)
I_step_run     = 200e-3;    % A — typisk 150-240 mA ved half-step på 5V

%% ---------- 3. STRØMTRÆK PR. STATE (refereret til 5V rail) -----------
% LDO er linear pass-through så 3.3V-laster trækker samme strøm på 5V-siden.

I_base_5V = I_esp_3v3 + I_oled_3v3 + I_lm35_5v + I_uln_idle;   % altid på

% Per state (A på 5V rail)
I_state.LOAD       = I_base_5V + I_hcsr04_idle;            % HC-SR04 ikke i brug
I_state.CHECK      = I_base_5V + I_hcsr04_avg;             % pinger hvert 15. min
I_state.EMPTY_ME   = I_base_5V + I_hcsr04_idle;            % bare venter
I_state.STEP_OPEN  = I_base_5V + I_hcsr04_idle + I_step_run;
I_state.STEP_CLOSE = I_base_5V + I_hcsr04_idle + I_step_run;
I_state.STEP_RESET = I_base_5V + I_hcsr04_idle + I_step_run;

%% ---------- 4. WEIGHTED AVERAGE OVER DØGN -----------------------------
states = fieldnames(I_state);
I_dt   = 0;          % integral af strøm × tid på 5V (A·s)
for k = 1:numel(states)
    I_dt = I_dt + I_state.(states{k}) * t.(states{k});
end
I_avg_5V = I_dt / T_day;                  % A — gennemsnit på 5V rail
P_avg_5V = V_rail * I_avg_5V;             % W på 5V rail

%% ---------- 5. BATTERILEVETID for hver batteri-konfiguration ----------
% Hjælpefunktion: tager P_5V (effekt på rail) + batt-struct,
% returnerer batterilevetid i timer.  Inkluderer buck quiescent + BMS.
calc_life = @(P5, b) (b.cap_mAh * 1e-3) / ...
            ( (P5/eta_buck + b.V_nom * (I_buck_quiet + b.I_BMS)) / b.V_nom );

batt.Alkaline_9V.life_h = calc_life(P_avg_5V, batt.Alkaline_9V);
batt.Li18650.life_h     = calc_life(P_avg_5V, batt.Li18650);

% Effekt + strøm på batteri-siden (informativt — for figur-titel)
batt.Alkaline_9V.I_batt = P_avg_5V/eta_buck/batt.Alkaline_9V.V_nom + I_buck_quiet;
batt.Li18650.I_batt     = P_avg_5V/eta_buck/batt.Li18650.V_nom + I_buck_quiet + batt.Li18650.I_BMS;

%% ---------- 6. STRØMBUDGET PR. KOMPONENT (24h gennemsnit) -------------
% til pie/breakdown-graf — beregner hvor mange mA HVER komponent koster
% i 24h-gennemsnit på 5V rail.

comp.ESP8266   = I_esp_3v3;                                                          % altid på
comp.OLED      = I_oled_3v3;                                                         % altid på
comp.LM35      = I_lm35_5v;                                                          % altid på
comp.HC_SR04   = ( t.CHECK*I_hcsr04_avg + (T_day - t.CHECK)*I_hcsr04_idle ) / T_day; % kun "aktiv" i CHECK
comp.Stepper   = ( (t.STEP_OPEN+t.STEP_CLOSE+t.STEP_RESET)*I_step_run ...
                 + (T_day - t.STEP_OPEN-t.STEP_CLOSE-t.STEP_RESET)*0 ) / T_day;
comp.ULN_quiet = I_uln_idle;                                                         % altid på

comp_names_disp = {'ESP8266 (WiFi)','OLED display','LM35 temp', ...
                   'HC-SR04','Stepper (28BYJ-48)','ULN2003 idle'};
comp_vals_mA = [comp.ESP8266 comp.OLED comp.LM35 comp.HC_SR04 comp.Stepper comp.ULN_quiet]*1e3;

%% ---------- 7. PRINT RESULTATER --------------------------------------
fprintf('\n========== POWER BUDGET (24h gennemsnit) ==========\n');
fprintf('Buck (MP1584): efficiency %.0f%%, quiescent %.2f mA\n', ...
        eta_buck*100, I_buck_quiet*1e3);
fprintf('Avg current @ 5V rail:  %.2f mA\n', I_avg_5V*1e3);
fprintf('Avg power  @ 5V rail:   %.0f mW\n', P_avg_5V*1e3);
fprintf('---------------------------------------------------\n');
fprintf('%-22s %5d mAh @ %.1fV =>  %.2f mA @ batt =>  %.1f h  (%.2f døgn)\n', ...
        batt.Alkaline_9V.label, batt.Alkaline_9V.cap_mAh, batt.Alkaline_9V.V_nom, ...
        batt.Alkaline_9V.I_batt*1e3, batt.Alkaline_9V.life_h, batt.Alkaline_9V.life_h/24);
fprintf('%-22s %5d mAh @ %.1fV =>  %.2f mA @ batt =>  %.1f h  (%.2f døgn)\n', ...
        '2x 18650 Li-ion', batt.Li18650.cap_mAh, batt.Li18650.V_nom, ...
        batt.Li18650.I_batt*1e3, batt.Li18650.life_h, batt.Li18650.life_h/24);
fprintf('===================================================\n\n');
fprintf('Komponent-fordeling (mA gns. på 5V rail):\n');
for k = 1:numel(comp_vals_mA)
    fprintf('  %-22s %6.2f mA  (%4.1f %%)\n', comp_names_disp{k}, ...
            comp_vals_mA(k), 100*comp_vals_mA(k)/sum(comp_vals_mA));
end
fprintf('\n');

%% ---------- 8. FIGUR TIL POSTER --------------------------------------
% Layout: 1 fig, 2 paneler — venstre: batterilevetid, højre: pie chart

fig = figure('Color','w','Units','pixels','Position',[100 100 1400 650]);
tl  = tiledlayout(fig,1,2,'TileSpacing','compact','Padding','compact');
%title(tl, sprintf('Power Budget'), ...
%        'FontSize',16,'FontWeight','bold');

% --- Panel 1: batterilevetid bar chart ---------------------------------
ax1 = nexttile;
life_h      = [batt.Alkaline_9V.life_h batt.Li18650.life_h];
batt_labels = {batt.Alkaline_9V.label, batt.Li18650.label};
colors      = [0.85 0.33 0.10;        % alkaline orange
               0.30 0.65 0.40];       % 18650 grøn
b = bar(ax1, life_h, 'FaceColor','flat','EdgeColor','none','BarWidth',0.55);
b.CData = colors;

set(ax1,'XTickLabel',batt_labels,'FontSize',13,'LineWidth',1.0,'Box','off', ...
        'TickLabelInterpreter','tex');
ylabel(ax1,'Battery-lifetime (hours)','FontSize',13);
title(ax1,'Expected lifetime','FontSize',14,'FontWeight','bold');
grid(ax1,'on'); ax1.YGrid='on'; ax1.XGrid='off';
ax1.GridAlpha = 0.25;

% Tekstetiketter ovenpå hver søjle
for k = 1:numel(life_h)
    if life_h(k) < 24
        lbl = sprintf('%.1f h', life_h(k));
    else
        lbl = sprintf('%.1f days', life_h(k)/24);
    end
    text(ax1, k, life_h(k)+max(life_h)*0.025, lbl, ...
        'HorizontalAlignment','center','FontSize',13,'FontWeight','bold');
end
ax1.YLim = [0 max(life_h)*1.25];

% --- Panel 2: horisontal bar chart med komponent-fordeling -------------
ax2 = nexttile;
% Sortér descending så største komponent øverst
[sort_vals, sidx] = sort(comp_vals_mA,'descend');
sort_names = comp_names_disp(sidx);
n = numel(sort_vals);

% Brug accent-blå for alle komponentbarer (samme som lithium-søjlen) ->
% visuel sammenhæng på posteren, og forskellen mellem komponenterne
% kommunikeres via barlængde, ikke farve.  Fremhæv ESP (den dominerende)
% i mørkere blå så den 'springer' frem.
barColors = repmat([0.45 0.70 0.90], n, 1);   % lys blå default
barColors(1,:) = [0.20 0.55 0.85];            % ESP — kraftig blå (samme som batteri)

bh = barh(ax2, 1:n, sort_vals, 'FaceColor','flat','EdgeColor','none','BarWidth',0.7);
bh.CData = barColors;
set(ax2, 'YDir', 'reverse');                  % største komponent øverst

set(ax2,'YTick', 1:n, 'YTickLabel', sort_names, ...
        'FontSize',12,'LineWidth',1.0,'Box','off');
xlabel(ax2,'Current draw on the 5V rail (mA, 24h avg.)','FontSize',13);
%title(ax2,'What uses the power?','FontSize',14,'FontWeight','bold');
ax2.XGrid='on'; ax2.YGrid='off'; ax2.GridAlpha = 0.25;
ax2.XLim = [0 max(sort_vals)*1.30];

% Tilføj mA + % til højre for hver bar
total_mA = sum(comp_vals_mA);
for k = 1:n
    pct_k = sort_vals(k)/total_mA*100;
    if sort_vals(k) >= 1
        lbl = sprintf('  %.1f mA  (%.1f %%)', sort_vals(k), pct_k);
    else
        lbl = sprintf('  %.2f mA  (%.2f %%)', sort_vals(k), pct_k);
    end
    text(ax2, sort_vals(k), k, lbl, ...
        'HorizontalAlignment','left','VerticalAlignment','middle', ...
        'FontSize',12,'FontWeight','bold','Color',[0.15 0.15 0.15]);
end

% --- Eksporter til PNG (300 DPI) -------------------------------------
out_dir = fullfile(fileparts(mfilename('fullpath')));
exportgraphics(fig, fullfile(out_dir,'battery_life.png'), 'Resolution',300);
fprintf('Figur gemt: %s\n', fullfile(out_dir,'battery_life.png'));

%% ====================================================================
%  9. WHAT-IF: ESP SLEEP MODES (kompatible med state machine)
%  --------------------------------------------------------------------
%  Deep-sleep ekskluderet: ESP8266 deep-sleep er en HARD RESET ved wake,
%  så hele state machine + bagsRemaining-tæller m.m. ville gå tabt.
%  Modem-sleep og light-sleep bevarer derimod programstate.
%
%    A) Som nu       : ESP altid 100% awake, WiFi associated
%    B) Modem-sleep  : CPU kører, RF kun tændt når der pushes til
%                      ThingSpeak.  State machine uændret.
%    C) Light-sleep  : CPU pauses mellem events, vækkes via timer
%                      eller GPIO-interrupt (knaptryk).  Kræver lidt
%                      mere firmware-arbejde men bevarer state machine.
% ====================================================================

% --- Sleep mode strømtræk (datasheet typiske værdier) -----------------
I_esp_active = I_esp_3v3;        % 75 mA — fuld WiFi associated
I_esp_modem  = 15e-3;            % A — modem sleep (RF off, CPU on)
I_esp_light  = 0.9e-3;           % A — light sleep (CPU paused, RTC kører)

% --- Antagelser om wake-aktivitet -------------------------------------
% Med 15-min polling i CHECK + tilsvarende push-interval:
%   Hver 15 min: WiFi op (~1.5s), HC-SR04 ping (~10ms), TS push (~0.5s),
%   alt i alt ~2s aktiv WiFi/CPU pr. 15 min cyklus.
T_event_cycle = T_check_poll;    % 15 min mellem hovedevents
t_event_act   = 2.0;             % sek aktiv pr. cyklus
duty_event    = t_event_act / T_event_cycle;

% Modem-sleep: CPU kører altid, RF kun aktiv under TS push.
I_esp_modem_avg = duty_event*I_esp_active + (1-duty_event)*I_esp_modem;
% Light-sleep: CPU pauset mellem events, vågner via timer/GPIO.
I_esp_light_avg = duty_event*I_esp_active + (1-duty_event)*I_esp_light;

% --- Definér 3 scenarier (alle med OLED stadig aktivt) ---------------
% Note: OLED kan også slukkes/dimes mellem events, men det er holdt udenfor
% her for at isolere effekten af ESP-sleep.

sc.A.label = 'No sleep';
sc.A.descr = 'WiFi always on';
sc.A.I_5V  = I_avg_5V;     % allerede beregnet ovenfor

sc.B.label = 'Modem-sleep';
sc.B.descr = 'WiFi RF only during TS push';
sc.B.I_5V  = I_esp_modem_avg + I_oled_3v3 + I_lm35_5v + I_hcsr04_idle + I_uln_idle;

sc.C.label = 'Light-sleep';
sc.C.descr = 'CPU paused between events';
sc.C.I_5V  = I_esp_light_avg + I_oled_3v3 + I_lm35_5v + I_hcsr04_idle + I_uln_idle;

% --- For hvert scenarie: levetid for begge batterityper --------------
sc_keys = {'A','B','C'};
for k = 1:numel(sc_keys)
    s     = sc.(sc_keys{k});
    P5    = V_rail * s.I_5V;
    s.life_alkaline_h = calc_life(P5, batt.Alkaline_9V);
    s.life_li18650_h  = calc_life(P5, batt.Li18650);
    s.I_5V_mA         = s.I_5V * 1e3;
    sc.(sc_keys{k})   = s;
end

% --- Print resultater -------------------------------------------------
fprintf('\n========== WHAT-IF: ESP SLEEP MODES ==========\n');
fprintf('Event-cycle = %d min, aktive per cycle = %.1f s\n', ...
        T_event_cycle/60, t_event_act);
fprintf('%-15s  %10s  %14s  %14s\n', ...
        'Scenario','I@5V (mA)','Alkaline 9V','2x 18650 7.2V');
fprintf('%s\n', repmat('-',1,62));
for k = 1:numel(sc_keys)
    s = sc.(sc_keys{k});
    if s.life_alkaline_h < 48
        s_alk = sprintf('%.1f h', s.life_alkaline_h);
    else
        s_alk = sprintf('%.1f day', s.life_alkaline_h/24);
    end
    if s.life_li18650_h < 48
        s_li = sprintf('%.1f h', s.life_li18650_h);
    else
        s_li = sprintf('%.1f day', s.life_li18650_h/24);
    end
    fprintf('%-15s  %8.2f    %14s  %14s\n', s.label, s.I_5V_mA, s_alk, s_li);
end
fprintf('===============================================\n\n');

%% ---------- 10. WHAT-IF FIGUR ---------------------------------------
fig2 = figure('Color','w','Units','pixels','Position',[100 100 1300 720]);
tl2  = tiledlayout(fig2,1,1,'Padding','compact');
%title(tl2, sprintf(['ESP-sleep mode vs batterytype']), ...
%        'FontSize',15,'FontWeight','bold');

ax3 = nexttile;
% Grupperet bar: én gruppe pr. scenarie, 2 søjler (alkaline + 18650)
life_matrix = zeros(numel(sc_keys),2);
sc_labels   = cell(numel(sc_keys),1);
sc_descrs   = cell(numel(sc_keys),1);
for k = 1:numel(sc_keys)
    s = sc.(sc_keys{k});
    life_matrix(k,1) = s.life_alkaline_h;
    life_matrix(k,2) = s.life_li18650_h;
    sc_labels{k} = s.label;
    sc_descrs{k} = s.descr;
end

bg = bar(ax3, life_matrix, 'BarWidth',0.85, 'EdgeColor','none');
bg(1).FaceColor = [0.85 0.33 0.10];   % alkaline orange
bg(2).FaceColor = [0.30 0.65 0.40];   % 18650 grøn

% Multi-line tick labels via TeX interpreter (\newline = ny linje)
combined_labels = cell(numel(sc_keys),1);
for k = 1:numel(sc_keys)
    combined_labels{k} = sprintf('\\bf%s\\rm\\newline\\fontsize{10}\\color[rgb]{0.4,0.4,0.4}\\it%s', ...
        sc_labels{k}, sc_descrs{k});
end
set(ax3,'XTick',1:numel(sc_keys),'XTickLabel',combined_labels, ...
        'TickLabelInterpreter','tex', ...
        'FontSize',13,'LineWidth',1.0,'Box','off');
ylabel(ax3,'Batterilevetid (log-skala)','FontSize',13);
ax3.YScale = 'log';
ax3.YGrid  = 'on'; ax3.GridAlpha = 0.3;
% Pænt log-tick udvalg — udvidet til 1 år (18650 + light sleep er meget langt)
ax3.YLim = [1 max(life_matrix(:))*3];
ax3.YTick = [1 6 12 24 72 168 720 1440 4320 8760];
ax3.YTickLabel = {'1 t','6 t','12 t','1 day','3 days','1 week','1 month','2 months','6 months','1 year'};

% Værdier ovenpå hver søjle
nGroups = size(life_matrix,1);
nBars   = size(life_matrix,2);
groupwidth = min(0.8, nBars/(nBars+1.5));
for i = 1:nBars
    x_offsets = (1:nGroups) - groupwidth/2 + (2*i-1)*groupwidth/(2*nBars);
    for j = 1:nGroups
        v = life_matrix(j,i);
        if v < 24
            lbl = sprintf('%.1f t', v);
        elseif v < 24*14
            lbl = sprintf('%.1f days', v/24);
        elseif v < 24*60
            lbl = sprintf('%.0f days', v/24);
        else
            lbl = sprintf('%.1f month', v/24/30);
        end
        text(ax3, x_offsets(j), v*1.18, lbl, ...
             'HorizontalAlignment','center','FontSize',11,'FontWeight','bold');
    end
end

% (Beskrivelser er nu indlejret i tick labels via TeX-interpreter ovenfor)

legend(ax3, {batt.Alkaline_9V.label, batt.Li18650.label}, ...
       'Location','northwest','FontSize',12,'Box','off', ...
       'Interpreter','tex');

% Annotation der viser "forbedringsfaktor" for B og C ift. A
factor_B = sc.B.life_alkaline_h / sc.A.life_alkaline_h;
factor_C = sc.C.life_alkaline_h / sc.A.life_alkaline_h;
% Multiplikator-annotation: placer LIDT TIL VENSTRE for søjlerne (mellem
% A og B og A og C visuelt) så de ikke dækker selve barerne
gm = @(a,b) sqrt(a.*b);
text(ax3, 1.5, gm(sc.A.life_alkaline_h, sc.B.life_alkaline_h)*1.4, ...
     sprintf('\\times %.0f', factor_B), ...
     'HorizontalAlignment','center','FontSize',22,'FontWeight','bold', ...
     'Color',[0.30 0.30 0.30]);
text(ax3, 2.5, gm(sc.A.life_alkaline_h, sc.C.life_alkaline_h)*1.4, ...
     sprintf('\\times %.0f', factor_C), ...
     'HorizontalAlignment','center','FontSize',22,'FontWeight','bold', ...
     'Color',[0.30 0.30 0.30]);
% Lille pil-effekt med tekst
% text(ax3, 1.5, gm(sc.A.life_alkaline_h, sc.B.life_alkaline_h)*0.65, ...
%    'forbedring','HorizontalAlignment','center','FontSize',9, ...
%    'FontAngle','italic','Color',[0.45 0.45 0.45]);
% text(ax3, 2.5, gm(sc.A.life_alkaline_h, sc.C.life_alkaline_h)*0.65, ...
%    'forbedring','HorizontalAlignment','center','FontSize',9, ...
%    'FontAngle','italic','Color',[0.45 0.45 0.45]);

exportgraphics(fig2, fullfile(out_dir,'whatif_sleepmodes.png'), 'Resolution',300);
fprintf('Figur gemt: %s\n', fullfile(out_dir,'whatif_sleepmodes.png'));
