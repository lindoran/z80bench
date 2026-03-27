; z80bench export — monz80_z88dk
; Generated: Thu Mar 26 14:12:18 2026
; Assembler: z88dk/z80asm

        INCLUDE "symbols.sym"
        ORG     0x0000


; ************************************
BLOCK COMMENTS!! 
Multiple Lines 

EVEN IN A BOX 
************************************
RESET_00:
        jp       00074h               ; RESET LEVEL 0
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
RESET_08:
        jp       07868h               ; RESET LEVEL 8
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        jp       07870h              
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        jp       07878h              
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        jp       07880h              
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        jp       07888h              
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        jp       07890h              
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        rst      38h                 
        ld       (07806h),hl         
        pop      hl                  
        dec      hl                  
        ld       (0780ch),hl         
        push     af                  
        pop      hl                  
        ld       (07800h),hl         
        ld       hl,00000h           
        add      hl,sp               
        ld       (0780eh),hl         
        ex       de,hl               
        ld       (07804h),hl         
        ld       h,b                 
        ld       l,c                 
        ld       (07802h),hl         
        ld       (07808h),ix         
        ld       (0780ah),iy         
        ld       hl,07811h           
        ld       b,008h              
        ld       e,(hl)              
        inc      hl                  
        ld       d,(hl)              
        inc      hl                  
        ld       a,d                 
        or       l                   
        jr       z,$+4               
        ld       a,(hl)              
        ld       (de),a              
        inc      hl                  
        djnz     $-11                
        call     0028fh              
        jr       $+117               
        ld       sp,0785fh           
        call     00f96h              
        ld       hl,07800h           
        ld       de,07829h           
        ld       (hl),000h           
        call     00255h              
        inc      hl                  
        jr       c,$-6               
        call     0050bh              
        DEFB     $0A,$0D             
        ld       c,l                 
        ld       c,a                 
        ld       c,(hl)              
        ld       e,d                 
        jr       c,$+50              
        jr       nz,$+88             
        ld       h,l                 
        ld       (hl),d              
        ld       (hl),e              
        ld       l,c                 
        ld       l,a                 
        ld       l,(hl)              
        jr       nz,$+51             
        ld       l,030h              
        ld       a,(bc)              
        dec      c                   
        ld       a,(bc)              
        ccf                          
        ld       b,e                 
        ld       c,a                 
        ld       d,b                 
        ld       e,c                 
        ld       l,054h              
        ld       e,b                 
        ld       d,h                 
        jr       nz,$+51             
        add      hl,sp               
        add      hl,sp               
        ld       (hl),02dh           
        ld       (03030h),a          
        scf                          
        jr       nz,$+70             
        ld       h,c                 
        halt                         
        ld       h,l                 
        jr       nz,$+70             
        ld       (hl),l              
        ld       l,(hl)              
        ld       h,(hl)              
        ld       l,c                 
        ld       h,l                 
        ld       l,h                 
        ld       h,h                 
        ld       a,(bc)              
        dec      c                   
        jr       nz,$+47             
        dec      l                   
        jr       nz,$+117            
        ld       h,l                 
        ld       h,l                 
        jr       nz,$+69             
        ld       c,a                 
        ld       d,b                 
        ld       e,c                 
        ld       l,054h              
        ld       e,b                 
        ld       d,h                 
        jr       nz,$+47             
        dec      l                   
        ld       l,00ah              
        nop                          
        ld       hl,07860h           
        ld       (0780ch),hl         
        ld       hl,0ffffh           
        ld       (0780eh),hl         
        ld       sp,0785fh           
        ld       a,060h              
        ld       (07810h),a          
        call     0050bh              
        ld       a,(bc)              
        dec      c                   
        ld       a,000h              
        ld       c,000h              
        ld       d,c                 
        ld       b,c                 
        call     00462h              
        ld       c,a                 
        ld       hl,00146h           
        ld       a,c                 
        cp       (hl)                
        inc      hl                  
        jr       nz,$+19             
        ld       a,b                 
        cp       (hl)                
        jr       nz,$+15             
        inc      hl                  
        call     00506h              
        ld       a,(hl)              
        inc      hl                  
        ld       h,(hl)              
        ld       l,a                 
        ld       bc,0012fh           
        push     bc                  
        jp       (hl)                
        ld       a,c                 
        cp       (hl)                
        jr       nz,$+3              
        inc      d                   
        inc      hl                  
        inc      hl                  
        inc      hl                  
        ld       a,(hl)              
        and      a                   
        jr       nz,$-32             
        or       b                   
        jr       nz,$+5              
        or       d                   
        jr       nz,$-46             
        call     0050bh              
        jr       nz,$+65             
        nop                          
        ld       a,(07810h)          
        and      010h                
        jr       z,$-77              
        ld       bc,00000h           
        call     00fa5h              
        and      a                   
        jr       nz,$-7              
        dec      bc                  
        ld       a,b                 
        or       c                   
        jr       nz,$-9              
        jr       $-93                
        ld       c,l                 
        ld       b,h                 
        ld       e,002h              
        ld       c,c                 
        ld       b,h                 
        dec      c                   
        ld       (bc),a              
        ld       d,d                 
        ld       b,h                 
        adc      a,a                 
        ld       (bc),a              
        ld       b,d                 
        ld       b,h                 
        ld       h,(hl)              
        inc      bc                  
        ld       d,d                 
        ld       b,d                 
        sbc      a,d                 
        inc      bc                  
        ld       b,l                 
        nop                          
        ld       a,(bc)              
        inc      bc                  
        ld       b,(hl)              
        nop                          
        ld       d,h                 
        inc      bc                  
        ld       c,c                 
        nop                          
        ld       l,c                 
        ld       (bc),a              
        ld       c,a                 
        nop                          
        ld       (hl),l              
        ld       (bc),a              
        ld       b,a                 
        nop                          
        push     bc                  
        ld       bc,00054h           
        add      a,d                 
        ld       (bc),a              
        ld       c,h                 
        nop                          
        cp       d                   
        inc      bc                  
        ld       b,(hl)              
        ld       b,c                 
        call     04302h              
        ld       b,d                 
        jp       nc,04502h           
        ld       b,h                 
        rst      10h                 
        ld       (bc),a              
        ld       c,h                 
        ld       c,b                 
        call     c,05802h            
        ld       c,c                 
        pop      hl                  
        ld       (bc),a              
        ld       e,c                 
        ld       c,c                 
        and      002h                
        ld       b,e                 
        ld       d,b                 
        ex       de,hl               
        ld       (bc),a              
        ld       d,b                 
        ld       d,e                 
        ret      p                   
        ld       (bc),a              
        ccf                          
        nop                          
        sbc      a,e                 
        ld       bc,02100h           
        rrca                         
        ld       c,0cdh              
        ret      nc                  
        inc      b                   
        ld       b,019h              
        ld       a,(hl)              
        inc      hl                  
        and      a                   
        jr       z,$+27              
        cp       07ch                
        jr       z,$+8               
        call     00fadh              
        dec      b                   
        jr       $-13                
        call     00506h              
        djnz     $-3                 
        ld       a,02dh              
        call     00fadh              
        call     00506h              
        jr       $-28                
        or       (hl)                
        jr       nz,$-36             
        ret                          
        ld       hl,(0780ch)         
        ld       b,h                 
        ld       c,l                 
        call     0049dh              
        ld       (0780ch),hl         
        call     004d0h              
        call     006cdh              
        ld       hl,07811h           
        ld       b,008h              
        ld       e,(hl)              
        inc      hl                  
        ld       d,(hl)              
        inc      hl                  
        ld       a,d                 
        or       l                   
        jr       z,$+7               
        ld       a,(de)              
        ld       (hl),a              
        ld       a,0ffh              
        ld       (de),a              
        inc      hl                  
        djnz     $-14                
        ld       ix,(07808h)         
        ld       iy,(0780ah)         
        ld       hl,(07802h)         
        ld       b,h                 
        ld       c,l                 
        ld       hl,(07804h)         
        ex       de,hl               
        ld       hl,(07800h)         
        push     hl                  
        pop      af                  
        ld       hl,(0780eh)         
        ld       sp,hl               
        ld       hl,(0780ch)         
        push     hl                  
        ld       hl,(07806h)         
        ret                          
        call     00489h              
        call     004d0h              
        call     00519h              
        call     00255h              
        jr       c,$-9               
        jr       z,$-11              
        ret                          
        call     00489h              
        call     004d0h              
        call     004aeh              
        call     00506h              
        ld       b,010h              
        push     hl                  
        call     00506h              
        ld       a,(hl)              
        call     004b3h              
        inc      hl                  
        ld       a,b                 
        dec      a                   
        and      003h                
        call     z,00506h            
        dec      b                   
        jr       nz,$-16             
        pop      hl                  
        call     00506h              
        ld       b,010h              
        ld       a,(hl)              
        call     0025bh              
        inc      hl                  
        dec      b                   
        jr       nz,$-6              
        call     00255h              
        jr       c,$-47              
        jr       z,$-49              
        ret                          
        ld       a,h                 
        cp       d                   
        ret      nz                  
        ld       a,l                 
        cp       e                   
        ret                          
        cp       020h                
        jr       c,$+7               
        cp       07fh                
        jp       c,00fadh            
        ld       a,02eh              
        jp       00fadh              
        call     00434h              
        ld       c,a                 
        call     00506h              
        in       a,(c)               
        jp       004b3h              
        call     00434h              
        ld       c,a                 
        call     00506h              
        call     00434h              
        out      (c),a               
        ret                          
        ld       hl,(0780ch)         
        ld       c,l                 
        call     00519h              
        call     004d0h              
        call     006d5h              
        ld       hl,002ach           
        ld       de,07800h           
        call     00506h              
        call     00510h              
        ld       a,(de)              
        ld       c,a                 
        inc      de                  
        ld       a,(de)              
        inc      de                  
        call     004b3h              
        ld       a,c                 
        call     004b3h              
        ld       a,(hl)              
        and      a                   
        jr       nz,$-20             
        ret                          
        ld       b,c                 
        ld       b,(hl)              
        dec      a                   
        nop                          
        ld       b,d                 
        ld       b,e                 
        dec      a                   
        nop                          
        ld       b,h                 
        ld       b,l                 
        dec      a                   
        nop                          
        ld       c,b                 
        ld       c,h                 
        dec      a                   
        nop                          
        ld       c,c                 
        ld       e,b                 
        dec      a                   
        nop                          
        ld       c,c                 
        ld       e,c                 
        dec      a                   
        nop                          
        ld       d,b                 
        ld       b,e                 
        dec      a                   
        nop                          
        ld       d,e                 
        ld       d,b                 
        dec      a                   
        nop                          
        nop                          
        ld       hl,07800h           
        jr       $+35                
        ld       hl,07802h           
        jr       $+30                
        ld       hl,07804h           
        jr       $+25                
        ld       hl,07806h           
        jr       $+20                
        ld       hl,07808h           
        jr       $+15                
        ld       hl,0780ah           
        jr       $+10                
        ld       hl,0780ch           
        jr       $+5                 
        ld       hl,0780eh           
        ld       d,h                 
        ld       e,l                 
        ld       a,(hl)              
        inc      hl                  
        ld       h,(hl)              
        ld       l,a                 
        call     004aeh              
        ld       a,02dh              
        call     00fadh              
        call     00480h              
        ld       a,l                 
        ld       (de),a              
        inc      de                  
        ld       a,h                 
        ld       (de),a              
        ret                          
        call     00480h              
        call     004d0h              
        call     004aeh              
        call     00506h              
        ld       a,(hl)              
        call     004b3h              
        ld       a,02dh              
        call     00fadh              
        call     00447h              
        jr       c,$+11              
        ld       (hl),a              
        inc      hl                  
        ld       a,l                 
        and      007h                
        jr       z,$-28              
        jr       $-24                
        cp       020h                
        jr       nz,$+7              
        call     00506h              
        jr       $-15                
        cp       027h                
        jr       nz,$+14             
        call     00fa5h              
        and      a                   
        jr       z,$-4               
        ld       (hl),a              
        call     0025bh              
        jr       $-31                
        cp       01bh                
        ret      z                   
        cp       00dh                
        ret      z                   
        cp       008h                
        jp       nz,00129h           
        dec      hl                  
        jr       $-69                
        call     00489h              
        call     00506h              
        call     00434h              
        ld       c,a                 
        ld       (hl),c              
        call     00255h              
        inc      hl                  
        jr       c,$-5               
        ret                          
        ld       de,07811h           
        ld       c,000h              
        call     0050bh              
        jr       nz,$+68             
        nop                          
        ld       a,c                 
        add      a,030h              
        call     00fadh              
        ld       a,03dh              
        call     00fadh              
        ld       a,(de)              
        ld       l,a                 
        inc      de                  
        ld       a,(de)              
        ld       h,a                 
        inc      de                  
        inc      de                  
        or       l                   
        jr       nz,$+12             
        call     0050bh              
        dec      l                   
        dec      l                   
        dec      l                   
        dec      l                   
        nop                          
        jr       $+5                 
        call     004aeh              
        inc      c                   
        ld       a,c                 
        cp       008h                
        jr       c,$-44              
        ret                          
        call     00462h              
        sub      030h                
        cp       008h                
        jp       nc,00129h           
        ld       e,a                 
        add      a,a                 
        add      a,e                 
        ld       d,078h              
        add      a,011h              
        ld       e,a                 
        jr       nc,$+3              
        inc      d                   
        call     00506h              
        call     00480h              
        ex       de,hl               
        ld       (hl),e              
        inc      hl                  
        ld       (hl),d              
        ret                          
        ld       a,030h              
        ld       (07810h),a          
        call     003c8h              
        jp       nz,00129h           
        jr       nc,$-6              
        ret                          
        call     00462h              
        cp       03ah                
        jr       z,$+57              
        cp       053h                
        jr       nz,$-9              
        call     00462h              
        cp       030h                
        jr       z,$-16              
        cp       039h                
        jr       z,$+83              
        cp       031h                
        jr       nz,$+81             
        call     00434h              
        ld       c,a                 
        sub      003h                
        ld       e,a                 
        call     00434h              
        ld       h,a                 
        add      a,c                 
        ld       c,a                 
        call     00434h              
        ld       l,a                 
        add      a,c                 
        ld       c,a                 
        call     00434h              
        ld       (hl),a              
        inc      hl                  
        add      a,c                 
        ld       c,a                 
        dec      e                   
        jr       nz,$-8              
        call     00434h              
        add      a,c                 
        inc      a                   
        and      a                   
        ret                          
        call     00434h              
        and      a                   
        jr       z,$+37              
        ld       c,a                 
        ld       e,a                 
        call     00434h              
        ld       h,a                 
        add      a,c                 
        ld       c,a                 
        call     00434h              
        ld       l,a                 
        add      a,c                 
        ld       c,a                 
        call     00434h              
        add      a,c                 
        ld       c,a                 
        call     00434h              
        ld       (hl),a              
        inc      hl                  
        add      a,c                 
        ld       c,a                 
        dec      e                   
        jr       nz,$-8              
        call     00434h              
        add      a,c                 
        and      a                   
        ret                          
        scf                          
        ret                          
        or       0ffh                
        ret                          
        call     0044dh              
        jp       c,00129h            
        rlca                         
        rlca                         
        rlca                         
        rlca                         
        ld       b,a                 
        call     0044dh              
        jp       c,00129h            
        or       b                   
        ret                          
        call     0044dh              
        jr       nc,$-16             
        ret                          
        call     00462h              
        cp       030h                
        ret      c                   
        sub      030h                
        cp       00ah                
        ccf                          
        ret      nc                  
        sub      007h                
        cp       00ah                
        ret      c                   
        cp       010h                
        ccf                          
        ret                          
        push     bc                  
        ld       a,(07810h)          
        ld       b,a                 
        call     00fa5h              
        and      a                   
        jr       z,$-4               
        bit      6,b                 
        jr       z,$+5               
        call     00fadh              
        bit      5,b                 
        jr       z,$+8               
        cp       061h                
        jr       c,$+4               
        and      05fh                
        pop      bc                  
        ret                          
        call     00434h              
        ld       h,a                 
        call     00434h              
        ld       l,a                 
        ret                          
        ld       bc,00000h           
        call     0049dh              
        ex       de,hl               
        ld       a,02ch              
        call     00fadh              
        ld       bc,0ffffh           
        call     0049dh              
        ex       de,hl               
        ret                          
        call     00447h              
        jr       nc,$-29             
        cp       020h                
        jp       nz,00129h           
        ld       a,008h              
        call     00fadh              
        ld       h,b                 
        ld       l,c                 
        ld       a,h                 
        call     004b3h              
        ld       a,l                 
        push     af                  
        rr       a                   
        rr       a                   
        rr       a                   
        rr       a                   
        call     004c0h              
        pop      af                  
        push     af                  
        and      00fh                
        cp       00ah                
        jr       c,$+4               
        add      a,007h              
        add      a,030h              
        call     00fadh              
        pop      af                  
        ret                          
        call     00fa5h              
        cp       01bh                
        jp       z,000e7h            
        cp       00dh                
        jr       nz,$+12             
        ld       a,(07810h)          
        and      07fh                
        ld       (07810h),a          
        jr       $+24                
        cp       020h                
        jr       nz,$+13             
        ld       a,(07810h)          
        xor      080h                
        jp       p,004fch            
        ld       (07810h),a          
        ld       a,(07810h)          
        and      a                   
        jp       m,004d0h            
        ld       a,00ah              
        call     00fadh              
        ld       a,00dh              
        jp       00fadh              
        ld       a,020h              
        jp       00fadh              
        pop      hl                  
        call     00510h              
        jp       (hl)                
        ld       a,(hl)              
        inc      hl                  
        and      a                   
        ret      z                   
        call     00fadh              
        jr       $-7                 
        ld       (07829h),hl         
        call     004aeh              
        push     de                  
        push     bc                  
        call     0057ch              
        ex       de,hl               
        ld       hl,(07829h)         
        ld       b,005h              
        call     00506h              
        ld       a,(hl)              
        inc      hl                  
        call     004b3h              
        dec      b                   
        call     00255h              
        jr       nz,$-12             
        call     00506h              
        call     00506h              
        call     00506h              
        djnz     $-9                 
        ld       hl,(07829h)         
        ld       b,008h              
        ld       a,(hl)              
        inc      hl                  
        call     0025bh              
        dec      b                   
        call     00255h              
        jr       nz,$-9              
        call     00506h              
        djnz     $-3                 
        ld       ix,0782dh           
        ld       a,(ix+000h)         
        and      a                   
        jr       z,$+25              
        inc      ix                  
        cp       020h                
        jr       z,$+8               
        call     00fadh              
        inc      b                   
        jr       $-16                
        call     00506h              
        inc      b                   
        ld       a,b                 
        and      007h                
        jr       nz,$-7              
        jr       $-27                
        pop      bc                  
        pop      de                  
        ret                          
        ld       a,(hl)              
        inc      hl                  
        ld       de,00acfh           
        cp       0cbh                
        jr       z,$+27              
        ld       de,00b19h           
        cp       0ddh                
        jr       z,$+20              
        ld       de,00d01h           
        cp       0edh                
        jr       z,$+13              
        ld       de,00c0dh           
        cp       0fdh                
        jr       z,$+6               
        ld       de,00910h           
        dec      hl                  
        ld       a,(hl)              
        inc      hl                  
        ld       (0782bh),a          
        ld       b,a                 
        ld       a,(de)              
        inc      de                  
        and      b                   
        ld       c,a                 
        ld       a,(de)              
        inc      de                  
        cp       c                   
        jr       z,$+12              
        ld       a,(de)              
        inc      de                  
        and      a                   
        jr       nz,$-3              
        ld       a,(de)              
        inc      de                  
        and      a                   
        jr       nz,$-15             
        ld       ix,0782dh           
        ld       a,(de)              
        and      a                   
        jp       z,006a9h            
        inc      de                  
        jp       m,006a1h            
        cp       073h                
        jr       nz,$+35             
        ld       a,(0782bh)          
        and      007h                
        ld       bc,00800h           
        push     hl                  
        ld       l,a                 
        ld       h,000h              
        add      hl,hl               
        add      hl,hl               
        add      hl,bc               
        ld       c,004h              
        ld       a,(hl)              
        and      a                   
        jr       z,$+11              
        inc      hl                  
        ld       (ix+000h),a         
        inc      ix                  
        dec      c                   
        jr       nz,$-11             
        pop      hl                  
        jr       $-44                
        cp       064h                
        jr       nz,$+10             
        ld       a,(0782bh)          
        rra                          
        rra                          
        rra                          
        jr       $-40                
        cp       070h                
        jr       nz,$+16             
        ld       bc,00820h           
        ld       a,(0782bh)          
        rra                          
        rra                          
        rra                          
        rra                          
        and      003h                
        jr       $-53                
        cp       062h                
        jr       nz,$+9              
        ld       a,(hl)              
        inc      hl                  
        call     006aeh              
        jr       $-85                
        cp       077h                
        jr       nz,$+15             
        ld       b,(hl)              
        inc      hl                  
        ld       a,(hl)              
        inc      hl                  
        call     006aeh              
        ld       a,b                 
        call     006aeh              
        jr       $-102               
        cp       078h                
        jr       nz,$+7              
        ld       bc,00830h           
        jr       $-46                
        cp       079h                
        jr       nz,$+7              
        ld       bc,00840h           
        jr       $-55                
        cp       063h                
        jr       nz,$+15             
        ld       a,(0782bh)          
        rra                          
        rra                          
        rra                          
        and      007h                
        ld       bc,00850h           
        jr       $-116               
        cp       072h                
        jr       nz,$+25             
        ld       a,(hl)              
        inc      hl                  
        ld       b,000h              
        and      a                   
        jp       p,00653h            
        dec      b                   
        add      a,l                 
        ld       c,a                 
        ld       a,h                 
        adc      a,b                 
        call     006aeh              
        ld       a,c                 
        call     006aeh              
        jp       005bbh              
        cp       07ah                
        jr       nz,$+31             
        ld       a,(hl)              
        ld       (0782ch),a          
        inc      hl                  
        ld       a,(hl)              
        ld       (0782bh),a          
        ld       b,a                 
        ld       a,(de)              
        and      b                   
        ld       c,a                 
        inc      de                  
        ld       a,(de)              
        inc      de                  
        cp       c                   
        jr       z,$+8               
        dec      hl                  
        ld       b,0cbh              
        jp       005adh              
        inc      hl                  
        jp       005bbh              
        cp       076h                
        jr       nz,$+7              
        ld       a,(0782ch)          
        jr       $-124               
        cp       06eh                
        jr       nz,$+12             
        ld       a,(0782bh)          
        rra                          
        rra                          
        rra                          
        and      007h                
        add      a,030h              
        ld       (ix+000h),a         
        inc      ix                  
        jp       005bbh              
        and      07fh                
        ld       bc,00870h           
        jp       005d0h              
        ld       (ix+000h),000h      
        ret                          
        push     af                  
        rr       a                   
        rr       a                   
        rr       a                   
        rr       a                   
        call     006bbh              
        pop      af                  
        push     af                  
        and      00fh                
        cp       00ah                
        jr       c,$+4               
        add      a,007h              
        add      a,030h              
        ld       (ix+000h),a         
        inc      ix                  
        pop      af                  
        ret                          
        ld       hl,(0780ch)         
        push     hl                  
        call     0057ch              
        pop      bc                  
        ld       a,l                 
        sub      c                   
        ld       c,a                 
        ld       b,000h              
        ld       hl,(0780ch)         
        ld       de,0782dh           
        ldir                         
        ld       (0780ch),hl         
        ex       de,hl               
        ld       (hl),0c3h           
        inc      hl                  
        ld       (hl),02eh           
        inc      hl                  
        ld       (hl),007h           
        ld       a,(0782dh)          
        ld       c,a                 
        ld       hl,00dcfh           
        ld       b,010h              
        ld       a,(hl)              
        inc      hl                  
        and      c                   
        cp       (hl)                
        inc      hl                  
        jr       z,$+45              
        inc      hl                  
        inc      hl                  
        djnz     $-9                 
        ld       hl,00000h           
        add      hl,sp               
        ld       (0782bh),hl         
        ld       ix,(07808h)         
        ld       iy,(0780ah)         
        ld       hl,(07802h)         
        ld       b,h                 
        ld       c,l                 
        ld       hl,(07804h)         
        ex       de,hl               
        ld       hl,(07800h)         
        push     hl                  
        pop      af                  
        ld       hl,(0780eh)         
        ld       sp,hl               
        ld       hl,(07806h)         
        jp       0782dh              
        ld       a,(hl)              
        inc      hl                  
        ld       h,(hl)              
        ld       l,a                 
        jp       (hl)                
        ld       (07806h),hl         
        push     af                  
        pop      hl                  
        ld       (07800h),hl         
        ld       hl,00000h           
        add      hl,sp               
        ld       (0780eh),hl         
        ex       de,hl               
        ld       (07804h),hl         
        ld       h,b                 
        ld       l,c                 
        ld       (07802h),hl         
        ld       (07808h),ix         
        ld       (0780ah),iy         
        ld       hl,(0782bh)         
        ld       sp,hl               
        ret                          
        ld       a,(0782eh)          
        cp       0e9h                
        jr       nz,$-86             
        ld       hl,(07808h)         
        jr       $+105               
        ld       a,(0782eh)          
        cp       0e9h                
        jr       nz,$-98             
        ld       hl,(0780ah)         
        jr       $+93                
        ld       a,c                 
        and      038h                
        ld       l,a                 
        ld       h,000h              
        ld       (0780ch),hl         
        ret                          
        ld       hl,(07806h)         
        jr       $+78                
        ld       c,018h              
        jr       $+12                
        ld       c,010h              
        jr       $+8                 
        ld       c,008h              
        jr       $+4                 
        ld       c,000h              
        call     007e0h              
        jr       nz,$+62             
        ld       hl,(0782eh)         
        jr       $+54                
        ld       a,(07803h)          
        dec      a                   
        ld       (07803h),a          
        jr       z,$+48              
        ld       a,(0782eh)          
        ld       c,a                 
        ld       b,000h              
        and      a                   
        jp       p,007a6h            
        dec      b                   
        ld       hl,(0780ch)         
        add      hl,bc               
        jr       $+28                
        call     007e0h              
        jr       nz,$+26             
        ld       hl,(0782eh)         
        ex       de,hl               
        ld       hl,(0780eh)         
        dec      hl                  
        ld       a,(0780dh)          
        ld       (hl),a              
        dec      hl                  
        ld       a,(0780ch)          
        ld       (hl),a              
        ld       (0780eh),hl         
        ex       de,hl               
        ld       (0780ch),hl         
        ret                          
        call     007e0h              
        jr       nz,$-4              
        ld       hl,(0780eh)         
        ld       a,(hl)              
        ld       (0780ch),a          
        inc      hl                  
        ld       a,(hl)              
        ld       (0780dh),a          
        inc      hl                  
        ld       (0780eh),hl         
        ret                          
        ld       a,c                 
        and      038h                
        or       0c2h                
        ld       (07831h),a          
        ld       hl,07835h           
        ld       (07832h),hl         
        ld       hl,0c93ch           
        ld       (07834h),hl         
        ld       hl,(07800h)         
        ld       h,000h              
        push     hl                  
        pop      af                  
        call     07831h              
        and      a                   
        ret                          
        ld       b,d                 
        nop                          
        nop                          
        nop                          
        ld       b,e                 
        nop                          
        nop                          
        nop                          
        ld       b,h                 
        nop                          
        nop                          
        nop                          
        ld       b,l                 
        nop                          
        nop                          
        nop                          
        ld       c,b                 
        nop                          
        nop                          
        nop                          
        ld       c,h                 
        nop                          
        nop                          
        nop                          
        jr       z,$+74              
        ld       c,h                 
        add      hl,hl               
        ld       b,c                 
        nop                          
        nop                          
        nop                          
        ld       b,d                 
        ld       b,e                 
        nop                          
        nop                          
        ld       b,h                 
        ld       b,l                 
        nop                          
        nop                          
        ld       c,b                 
        ld       c,h                 
        nop                          
        nop                          
        ld       d,e                 
        ld       d,b                 
        nop                          
        nop                          
        ld       b,d                 
        ld       b,e                 
        nop                          
        nop                          
        ld       b,h                 
        ld       b,l                 
        nop                          
        nop                          
        ld       c,c                 
        ld       e,b                 
        nop                          
        nop                          
        ld       d,e                 
        ld       d,b                 
        nop                          
        nop                          
        ld       b,d                 
        ld       b,e                 
        nop                          
        nop                          
        ld       b,h                 
        ld       b,l                 
        nop                          
        nop                          
        ld       c,c                 
        ld       e,c                 
        nop                          
        nop                          
        ld       d,e                 
        ld       d,b                 
        nop                          
        nop                          
        ld       c,(hl)              
        ld       e,d                 
        nop                          
        nop                          
        ld       e,d                 
        nop                          
        nop                          
        nop                          
        ld       c,(hl)              
        ld       b,e                 
        nop                          
        nop                          
        ld       b,e                 
        nop                          
        nop                          
        nop                          
        ld       d,b                 
        ld       c,a                 
        nop                          
        nop                          
        ld       d,b                 
        ld       b,l                 
        nop                          
        nop                          
        ld       d,b                 
        nop                          
        nop                          
        nop                          
        ld       c,l                 
        nop                          
        nop                          
        nop                          
        ld       c,h                 
        ld       b,h                 
        jr       nz,$+2              
        ld       b,d                 
        ld       b,e                 
        nop                          
        nop                          
        ld       b,h                 
        ld       b,l                 
        nop                          
        nop                          
        ld       c,b                 
        ld       c,h                 
        nop                          
        nop                          
        ld       c,c                 
        ld       e,b                 
        nop                          
        nop                          
        ld       c,c                 
        ld       e,c                 
        nop                          
        nop                          
        jr       z,$+68              
        ld       b,e                 
        add      hl,hl               
        jr       z,$+70              
        ld       b,l                 
        add      hl,hl               
        jr       z,$+74              
        ld       c,h                 
        add      hl,hl               
        jr       z,$+75              
        ld       e,b                 
        dec      hl                  
        jr       z,$+75              
        ld       e,c                 
        dec      hl                  
        ld       b,c                 
        inc      l                   
        nop                          
        nop                          
        inc      l                   
        ld       b,c                 
        nop                          
        nop                          
        ld       d,e                 
        ld       d,b                 
        nop                          
        nop                          
        ld       d,b                 
        ld       d,l                 
        ld       d,e                 
        ld       c,b                 
        ld       d,b                 
        ld       c,a                 
        ld       d,b                 
        jr       nz,$+67             
        ld       b,(hl)              
        nop                          
        nop                          
        ld       b,l                 
        ld       e,b                 
        nop                          
        nop                          
        ld       c,h                 
        ld       b,h                 
        nop                          
        nop                          
        ld       b,e                 
        ld       d,b                 
        nop                          
        nop                          
        ld       b,c                 
        ld       b,h                 
        ld       b,h                 
        jr       nz,$+67             
        ld       b,h                 
        ld       b,e                 
        jr       nz,$+85             
        ld       d,l                 
        ld       b,d                 
        jr       nz,$+85             
        ld       b,d                 
        ld       b,e                 
        jr       nz,$+67             
        ld       c,(hl)              
        ld       b,h                 
        jr       nz,$+81             
        ld       d,d                 
        jr       nz,$+2              
        ld       e,b                 
        ld       c,a                 
        ld       d,d                 
        jr       nz,$+75             
        ld       c,(hl)              
        ld       b,e                 
        jr       nz,$+70             
        ld       b,l                 
        ld       b,e                 
        jr       nz,$+84             
        ld       c,h                 
        nop                          
        nop                          
        ld       d,d                 
        ld       d,d                 
        nop                          
        nop                          
        ld       c,d                 
        ld       d,b                 
        jr       nz,$+2              
        ld       c,d                 
        ld       d,d                 
        jr       nz,$+2              
        ld       b,e                 
        ld       b,c                 
        ld       c,h                 
        ld       c,h                 
        ld       d,d                 
        ld       b,l                 
        ld       d,h                 
        nop                          
        ld       c,c                 
        ld       c,(hl)              
        nop                          
        nop                          
        ld       c,a                 
        ld       d,l                 
        ld       d,h                 
        nop                          
        ld       b,d                 
        ld       c,c                 
        ld       d,h                 
        jr       nz,$+85             
        ld       b,l                 
        ld       d,h                 
        jr       nz,$+84             
        ld       b,l                 
        ld       d,e                 
        jr       nz,$+1              
        halt                         
        ld       c,b                 
        ld       b,c                 
        ld       c,h                 
        ld       d,h                 
        nop                          
        ret      nz                  
        ld       b,b                 
        add      a,b                 
        ld       h,h                 
        inc      l                   
        ld       (hl),e              
        nop                          
        rst      0                   
        ld       b,080h              
        ld       h,h                 
        inc      l                   
        ld       h,d                 
        nop                          
        rst      38h                 
        ld       a,(bc)              
        add      a,b                 
        adc      a,e                 
        add      a,(hl)              
        nop                          
        rst      38h                 
        ld       a,(de)              
        add      a,b                 
        adc      a,e                 
        add      a,a                 
        nop                          
        rst      38h                 
        ld       a,(08b80h)          
        jr       z,$+121             
        add      hl,hl               
        nop                          
        rst      38h                 
        ld       (bc),a              
        add      a,b                 
        add      a,(hl)              
        adc      a,h                 
        nop                          
        rst      38h                 
        ld       (de),a              
        add      a,b                 
        add      a,a                 
        adc      a,h                 
        nop                          
        rst      8                   
        ld       bc,07080h           
        inc      l                   
        ld       (hl),a              
        nop                          
        rst      38h                 
        ld       (02880h),a          
        ld       (hl),a              
        add      hl,hl               
        adc      a,h                 
        nop                          
        rst      38h                 
        ld       hl,(08380h)         
        inc      l                   
        jr       z,$+121             
        add      hl,hl               
        nop                          
        rst      38h                 
        ld       (02880h),hl         
        ld       (hl),a              
        add      hl,hl               
        inc      l                   
        add      a,e                 
        nop                          
        rst      38h                 
        ld       sp,hl               
        add      a,b                 
        adc      a,l                 
        inc      l                   
        add      a,e                 
        nop                          
        rst      38h                 
        push     af                  
        adc      a,(hl)              
        jr       nz,$-110            
        nop                          
        rst      8                   
        push     bc                  
        adc      a,(hl)              
        jr       nz,$+114            
        nop                          
        rst      38h                 
        pop      af                  
        adc      a,a                 
        sub      b                   
        nop                          
        rst      8                   
        pop      bc                  
        adc      a,a                 
        ld       (hl),b              
        nop                          
        rst      38h                 
        ex       de,hl               
        sub      c                   
        jr       nz,$-124            
        inc      l                   
        add      a,e                 
        nop                          
        rst      38h                 
        ex       af,af'              
        sub      c                   
        jr       nz,$-110            
        inc      l                   
        sub      b                   
        daa                          
        nop                          
        rst      38h                 
        exx                          
        sub      c                   
        ld       e,b                 
        nop                          
        rst      38h                 
        ex       (sp),hl             
        sub      c                   
        jr       nz,$+42             
        adc      a,l                 
        add      hl,hl               
        inc      l                   
        add      a,e                 
        nop                          
        ret      m                   
        add      a,b                 
        sub      h                   
        adc      a,e                 
        ld       (hl),e              
        nop                          
        rst      38h                 
        add      a,094h              
        adc      a,e                 
        ld       h,d                 
        nop                          
        ret      m                   
        adc      a,b                 
        sub      l                   
        adc      a,e                 
        ld       (hl),e              
        nop                          
        rst      38h                 
        adc      a,095h              
        adc      a,e                 
        ld       h,d                 
        nop                          
        ret      m                   
        sub      b                   
        sub      (hl)                
        adc      a,e                 
        ld       (hl),e              
        nop                          
        rst      38h                 
        sub      096h                
        adc      a,e                 
        ld       h,d                 
        nop                          
        ret      m                   
        sbc      a,b                 
        sub      a                   
        adc      a,e                 
        ld       (hl),e              
        nop                          
        rst      38h                 
        sbc      a,097h              
        adc      a,e                 
        ld       h,d                 
        nop                          
        ret      m                   
        and      b                   
        sbc      a,b                 
        adc      a,e                 
        ld       (hl),e              
        nop                          
        rst      38h                 
        and      098h                
        adc      a,e                 
        ld       h,d                 
        nop                          
        ret      m                   
        xor      b                   
        sbc      a,d                 
        adc      a,e                 
        ld       (hl),e              
        nop                          
        rst      38h                 
        xor      09ah                
        adc      a,e                 
        ld       h,d                 
        nop                          
        ret      m                   
        or       b                   
        sbc      a,c                 
        adc      a,e                 
        ld       (hl),e              
        nop                          
        rst      38h                 
        or       099h                
        adc      a,e                 
        ld       h,d                 
        nop                          
        ret      m                   
        cp       b                   
        sub      e                   
        jr       nz,$-115            
        ld       (hl),e              
        nop                          
        rst      38h                 
        cp       093h                
        jr       nz,$-115            
        ld       h,d                 
        nop                          
        rst      0                   
        inc      b                   
        sbc      a,e                 
        ld       h,h                 
        nop                          
        rst      0                   
        dec      b                   
        sbc      a,h                 
        ld       h,h                 
        nop                          
        rst      8                   
        add      hl,bc               
        sub      h                   
        add      a,e                 
        inc      l                   
        ld       (hl),b              
        nop                          
        rst      38h                 
        daa                          
        ld       b,h                 
        ld       b,c                 
        ld       b,c                 
        nop                          
        rst      38h                 
        cpl                          
        sub      e                   
        ld       c,h                 
        nop                          
        rst      38h                 
        ccf                          
        ld       b,e                 
        ld       b,e                 
        ld       b,(hl)              
        nop                          
        rst      38h                 
        scf                          
        ld       d,e                 
        ld       b,e                 
        ld       b,(hl)              
        nop                          
        rst      38h                 
        nop                          
        ld       c,(hl)              
        ld       c,a                 
        ld       d,b                 
        nop                          
        rst      38h                 
        di                           
        ld       b,h                 
        ld       c,c                 
        nop                          
        rst      38h                 
        ei                           
        ld       b,l                 
        ld       c,c                 
        nop                          
        rst      8                   
        inc      bc                  
        sbc      a,e                 
        ld       (hl),b              
        nop                          
        rst      8                   
        dec      bc                  
        sbc      a,h                 
        ld       (hl),b              
        nop                          
        rst      38h                 
        rlca                         
        sbc      a,l                 
        ld       b,e                 
        ld       b,c                 
        nop                          
        rst      38h                 
        rla                          
        sbc      a,l                 
        ld       b,c                 
        nop                          
        rst      38h                 
        rrca                         
        sbc      a,(hl)              
        ld       b,e                 
        ld       b,c                 
        nop                          
        rst      38h                 
        rra                          
        sbc      a,(hl)              
        ld       b,c                 
        nop                          
        rst      38h                 
        jp       0779fh              
        nop                          
        rst      0                   
        jp       nz,0639fh           
        inc      l                   
        ld       (hl),a              
        nop                          
        rst      38h                 
        jr       $-94                
        ld       (hl),d              
        nop                          
        rst      38h                 
        jr       c,$-94              
        ld       b,e                 
        inc      l                   
        ld       (hl),d              
        nop                          
        rst      38h                 
        jr       nc,$-94             
        ld       c,(hl)              
        ld       b,e                 
        inc      l                   
        ld       (hl),d              
        nop                          
        rst      38h                 
        jr       z,$-94              
        ld       e,d                 
        inc      l                   
        ld       (hl),d              
        nop                          
        rst      38h                 
        jr       nz,$-94             
        ld       c,(hl)              
        ld       e,d                 
        inc      l                   
        ld       (hl),d              
        nop                          
        rst      38h                 
        jp       (hl)                
        sbc      a,a                 
        adc      a,b                 
        nop                          
        rst      38h                 
        djnz     $+70                
        ld       c,d                 
        ld       c,(hl)              
        ld       e,d                 
        jr       nz,$+116            
        nop                          
        rst      38h                 
        call     020a1h              
        ld       (hl),a              
        nop                          
        rst      0                   
        call     nz,020a1h           
        ld       h,e                 
        inc      l                   
        ld       (hl),a              
        nop                          
        rst      38h                 
        ret                          
        and      d                   
        nop                          
        rst      0                   
        ret      nz                  
        and      d                   
        jr       nz,$+101            
        nop                          
        rst      0                   
        rst      0                   
        ld       d,d                 
        ld       d,e                 
        ld       d,h                 
        jr       nz,$+112            
        nop                          
        rst      38h                 
        in       a,(0a3h)            
        jr       nz,$-115            
        jr       z,$+100             
        add      hl,hl               
        nop                          
        rst      38h                 
        out      (0a4h),a            
        jr       nz,$+42             
        ld       h,d                 
        add      hl,hl               
        adc      a,h                 
        nop                          
        nop                          
        ccf                          
        nop                          
        ret      m                   
        nop                          
        sbc      a,l                 
        ld       b,e                 
        jr       nz,$+117            
        nop                          
        ret      m                   
        djnz     $-97                
        jr       nz,$+117            
        nop                          
        ret      m                   
        ex       af,af'              
        sbc      a,(hl)              
        ld       b,e                 
        jr       nz,$+117            
        nop                          
        ret      m                   
        jr       $-96                
        jr       nz,$+117            
        nop                          
        ret      m                   
        jr       nz,$+85             
        ld       c,h                 
        ld       b,c                 
        jr       nz,$+117            
        nop                          
        ret      m                   
        jr       z,$+85              
        ld       d,d                 
        ld       b,c                 
        jr       nz,$+117            
        nop                          
        ret      m                   
        jr       c,$+85              
        ld       d,d                 
        ld       c,h                 
        jr       nz,$+117            
        nop                          
        ret      nz                  
        ld       b,b                 
        and      l                   
        ld       l,(hl)              
        inc      l                   
        ld       (hl),e              
        nop                          
        ret      nz                  
        ret      nz                  
        and      (hl)                
        ld       l,(hl)              
        inc      l                   
        ld       (hl),e              
        nop                          
        ret      nz                  
        add      a,b                 
        and      a                   
        ld       l,(hl)              
        inc      l                   
        ld       (hl),e              
        nop                          
        nop                          
        ccf                          
        nop                          
        rst      0                   
        ld       b,(hl)              
        add      a,b                 
        ld       h,h                 
        inc      l                   
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        ret      m                   
        ld       (hl),b              
        add      a,b                 
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        inc      l                   
        ld       (hl),e              
        nop                          
        rst      38h                 
        ld       (hl),080h           
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        inc      l                   
        ld       h,d                 
        nop                          
        rst      38h                 
        ld       hl,08480h           
        inc      l                   
        ld       (hl),a              
        nop                          
        rst      38h                 
        ld       hl,(08480h)         
        inc      l                   
        jr       z,$+121             
        add      hl,hl               
        nop                          
        rst      38h                 
        ld       (02880h),hl         
        ld       (hl),a              
        add      hl,hl               
        inc      l                   
        add      a,h                 
        nop                          
        rst      38h                 
        ld       sp,hl               
        add      a,b                 
        adc      a,l                 
        inc      l                   
        add      a,h                 
        nop                          
        rst      38h                 
        push     hl                  
        adc      a,(hl)              
        jr       nz,$-122            
        nop                          
        rst      38h                 
        pop      hl                  
        adc      a,a                 
        add      a,h                 
        nop                          
        rst      38h                 
        ex       (sp),hl             
        sub      c                   
        jr       nz,$+42             
        adc      a,l                 
        add      hl,hl               
        inc      l                   
        add      a,h                 
        nop                          
        rst      38h                 
        add      a,(hl)              
        sub      h                   
        adc      a,e                 
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        adc      a,(hl)              
        sub      l                   
        adc      a,e                 
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        sub      (hl)                
        sub      (hl)                
        adc      a,e                 
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        sbc      a,(hl)              
        sub      a                   
        adc      a,e                 
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        and      (hl)                
        sbc      a,b                 
        adc      a,e                 
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        xor      (hl)                
        sbc      a,d                 
        adc      a,e                 
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        or       (hl)                
        sbc      a,c                 
        adc      a,e                 
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        cp       (hl)                
        sub      e                   
        jr       nz,$-115            
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        inc      (hl)                
        sbc      a,e                 
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        dec      (hl)                
        sbc      a,h                 
        adc      a,c                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      8                   
        add      hl,bc               
        sub      h                   
        add      a,h                 
        inc      l                   
        ld       a,b                 
        nop                          
        rst      38h                 
        inc      hl                  
        sbc      a,e                 
        add      a,h                 
        nop                          
        rst      38h                 
        dec      hl                  
        sbc      a,h                 
        add      a,h                 
        nop                          
        rst      38h                 
        jp       (hl)                
        sbc      a,a                 
        adc      a,c                 
        nop                          
        rst      38h                 
        bit      7,d                 
        rst      38h                 
        ld       b,09dh              
        ld       b,e                 
        jr       nz,$-117            
        halt                         
        add      hl,hl               
        nop                          
        rst      38h                 
        bit      7,d                 
        rst      38h                 
        ld       d,09eh              
        ld       b,e                 
        jr       nz,$-117            
        halt                         
        add      hl,hl               
        nop                          
        rst      38h                 
        bit      7,d                 
        rst      0                   
        ld       b,(hl)              
        and      l                   
        ld       l,(hl)              
        inc      l                   
        adc      a,c                 
        halt                         
        add      hl,hl               
        nop                          
        rst      38h                 
        bit      7,d                 
        rst      0                   
        add      a,0a6h              
        ld       l,(hl)              
        inc      l                   
        adc      a,c                 
        halt                         
        add      hl,hl               
        nop                          
        rst      38h                 
        bit      7,d                 
        rst      0                   
        add      a,(hl)              
        and      a                   
        ld       l,(hl)              
        inc      l                   
        adc      a,c                 
        halt                         
        add      hl,hl               
        nop                          
        nop                          
        ccf                          
        nop                          
        rst      0                   
        ld       b,(hl)              
        add      a,b                 
        ld       h,h                 
        inc      l                   
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        ret      m                   
        ld       (hl),b              
        add      a,b                 
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        inc      l                   
        ld       (hl),e              
        nop                          
        rst      38h                 
        ld       (hl),080h           
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        inc      l                   
        ld       h,d                 
        nop                          
        rst      38h                 
        ld       hl,08580h           
        inc      l                   
        ld       (hl),a              
        nop                          
        rst      38h                 
        ld       hl,(08580h)         
        inc      l                   
        jr       z,$+121             
        add      hl,hl               
        nop                          
        rst      38h                 
        ld       (02880h),hl         
        ld       (hl),a              
        add      hl,hl               
        inc      l                   
        add      a,l                 
        nop                          
        rst      38h                 
        ld       sp,hl               
        add      a,b                 
        adc      a,l                 
        inc      l                   
        add      a,l                 
        nop                          
        rst      38h                 
        push     hl                  
        adc      a,(hl)              
        jr       nz,$-121            
        nop                          
        rst      38h                 
        pop      hl                  
        adc      a,a                 
        add      a,l                 
        nop                          
        rst      38h                 
        ex       (sp),hl             
        sub      c                   
        jr       nz,$+42             
        adc      a,l                 
        add      hl,hl               
        inc      l                   
        add      a,l                 
        nop                          
        rst      38h                 
        add      a,(hl)              
        sub      h                   
        adc      a,e                 
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        adc      a,(hl)              
        sub      l                   
        adc      a,e                 
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        sub      (hl)                
        sub      (hl)                
        adc      a,e                 
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        sbc      a,(hl)              
        sub      a                   
        adc      a,e                 
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        and      (hl)                
        sbc      a,b                 
        adc      a,e                 
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        xor      (hl)                
        sbc      a,d                 
        adc      a,e                 
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        or       (hl)                
        sbc      a,c                 
        adc      a,e                 
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        cp       (hl)                
        sub      e                   
        jr       nz,$-115            
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        inc      (hl)                
        sbc      a,e                 
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      38h                 
        dec      (hl)                
        sbc      a,h                 
        adc      a,d                 
        ld       h,d                 
        add      hl,hl               
        nop                          
        rst      8                   
        add      hl,bc               
        sub      h                   
        add      a,l                 
        inc      l                   
        ld       a,c                 
        nop                          
        rst      38h                 
        inc      hl                  
        sbc      a,e                 
        add      a,l                 
        nop                          
        rst      38h                 
        dec      hl                  
        sbc      a,h                 
        add      a,l                 
        nop                          
        rst      38h                 
        jp       (hl)                
        sbc      a,a                 
        adc      a,d                 
        nop                          
        rst      38h                 
        bit      7,d                 
        rst      38h                 
        ld       b,09dh              
        ld       b,e                 
        jr       nz,$-116            
        halt                         
        add      hl,hl               
        nop                          
        rst      38h                 
        bit      7,d                 
        rst      38h                 
        ld       d,09eh              
        ld       b,e                 
        jr       nz,$-116            
        halt                         
        add      hl,hl               
        nop                          
        rst      38h                 
        bit      7,d                 
        rst      0                   
        ld       b,(hl)              
        and      l                   
        ld       l,(hl)              
        inc      l                   
        adc      a,d                 
        halt                         
        add      hl,hl               
        nop                          
        rst      38h                 
        bit      7,d                 
        rst      0                   
        add      a,0a6h              
        ld       l,(hl)              
        inc      l                   
        adc      a,d                 
        halt                         
        add      hl,hl               
        nop                          
        rst      38h                 
        bit      7,d                 
        rst      0                   
        add      a,(hl)              
        and      a                   
        ld       l,(hl)              
        inc      l                   
        adc      a,d                 
        halt                         
        add      hl,hl               
        nop                          
        nop                          
        ccf                          
        nop                          
        rst      38h                 
        ld       d,a                 
        add      a,b                 
        adc      a,e                 
        ld       c,c                 
        nop                          
        rst      38h                 
        ld       e,a                 
        add      a,b                 
        adc      a,e                 
        ld       d,d                 
        nop                          
        rst      38h                 
        ld       b,a                 
        add      a,b                 
        ld       c,c                 
        adc      a,h                 
        nop                          
        rst      38h                 
        ld       c,a                 
        add      a,b                 
        ld       d,d                 
        adc      a,h                 
        nop                          
        rst      8                   
        ld       c,e                 
        add      a,b                 
        ld       (hl),b              
        jr       z,$+121             
        add      hl,hl               
        nop                          
        rst      38h                 
        and      b                   
        sub      d                   
        ld       c,c                 
        nop                          
        rst      38h                 
        or       b                   
        sub      d                   
        ld       c,c                 
        ld       d,d                 
        nop                          
        rst      38h                 
        xor      b                   
        sub      d                   
        ld       b,h                 
        nop                          
        rst      38h                 
        cp       b                   
        sub      d                   
        ld       b,h                 
        ld       d,d                 
        nop                          
        rst      38h                 
        and      c                   
        sub      e                   
        ld       c,c                 
        nop                          
        rst      38h                 
        or       c                   
        sub      e                   
        ld       c,c                 
        ld       d,d                 
        nop                          
        rst      38h                 
        xor      c                   
        sub      e                   
        ld       b,h                 
        nop                          
        rst      38h                 
        cp       c                   
        sub      e                   
        ld       b,h                 
        ld       d,d                 
        nop                          
        rst      38h                 
        ld       b,h                 
        ld       c,(hl)              
        ld       b,l                 
        ld       b,a                 
        nop                          
        rst      38h                 
        ld       b,(hl)              
        ld       c,c                 
        ld       c,l                 
        jr       nz,$+50             
        nop                          
        rst      38h                 
        ld       d,(hl)              
        ld       c,c                 
        ld       c,l                 
        jr       nz,$+51             
        nop                          
        rst      38h                 
        ld       e,(hl)              
        ld       c,c                 
        ld       c,l                 
        jr       nz,$+52             
        nop                          
        rst      8                   
        ld       c,d                 
        sub      l                   
        add      a,e                 
        inc      l                   
        ld       (hl),b              
        nop                          
        rst      8                   
        ld       b,d                 
        sub      a                   
        add      a,e                 
        inc      l                   
        ld       (hl),b              
        nop                          
        rst      38h                 
        ld       l,a                 
        sbc      a,l                 
        ld       b,h                 
        nop                          
        rst      38h                 
        ld       h,a                 
        sbc      a,(hl)              
        ld       b,h                 
        nop                          
        rst      38h                 
        ld       c,l                 
        and      d                   
        ld       c,c                 
        nop                          
        rst      38h                 
        ld       b,l                 
        and      d                   
        ld       c,(hl)              
        nop                          
        rst      0                   
        ld       b,b                 
        and      e                   
        jr       nz,$+102            
        inc      l                   
        jr       z,$+69              
        add      hl,hl               
        nop                          
        rst      38h                 
        and      d                   
        and      e                   
        ld       c,c                 
        nop                          
        rst      38h                 
        or       d                   
        and      e                   
        ld       c,c                 
        ld       d,d                 
        nop                          
        rst      38h                 
        xor      d                   
        and      e                   
        ld       b,h                 
        nop                          
        rst      38h                 
        cp       d                   
        and      e                   
        ld       b,h                 
        ld       d,d                 
        nop                          
        rst      0                   
        ld       b,c                 
        and      h                   
        jr       nz,$+42             
        ld       b,e                 
        add      hl,hl               
        inc      l                   
        ld       h,h                 
        nop                          
        rst      38h                 
        and      e                   
        and      h                   
        ld       c,c                 
        nop                          
        rst      38h                 
        or       e                   
        ld       c,a                 
        ld       d,h                 
        ld       c,c                 
        ld       d,d                 
        nop                          
        rst      38h                 
        xor      e                   
        and      h                   
        ld       b,h                 
        nop                          
        rst      38h                 
        cp       e                   
        ld       c,a                 
        ld       d,h                 
        ld       b,h                 
        ld       d,d                 
        nop                          
        nop                          
        ccf                          
        nop                          
        rst      38h                 
        jp       0078dh              
        rst      0                   
        jp       nz,00788h           
        rst      38h                 
        jr       $-99                
        rlca                         
        rst      38h                 
        jp       (hl)                
        ld       (hl),l              
        rlca                         
        rst      38h                 
        call     007b1h              
        rst      0                   
        call     nz,007ach           
        rst      38h                 
        ret                          
        rst      8                   
        rlca                         
        rst      0                   
        ret      nz                  
        jp       z,0ff07h            
        djnz     $-108               
        rlca                         
        rst      38h                 
        jr       c,$+124             
        rlca                         
        rst      38h                 
        jr       nc,$+128            
        rlca                         
        rst      38h                 
        jr       z,$-124             
        rlca                         
        rst      38h                 
        jr       nz,$-120            
        rlca                         
        rst      0                   
        rst      0                   
        ld       l,e                 
        rlca                         
        rst      38h                 
        rst      38h                 
        cp       05fh                
        rlca                         
        ld       c,l                 
        ld       c,a                 
        ld       c,(hl)              
        ld       e,d                 
        jr       c,$+50              
        jr       nz,$+69             
        ld       l,a                 
        ld       l,l                 
        ld       l,l                 
        ld       h,c                 
        ld       l,(hl)              
        ld       h,h                 
        ld       (hl),e              
        ld       a,(0000ah)          
        ld       b,d                 
        ld       d,d                 
        jr       nz,$+50             
        dec      l                   
        scf                          
        jr       nz,$+99             
        ld       h,h                 
        ld       h,h                 
        ld       (hl),d              
        ld       a,h                 
        ld       d,e                 
        ld       h,l                 
        ld       (hl),h              
        jr       nz,$+100            
        ld       (hl),d              
        ld       h,l                 
        ld       h,c                 
        ld       l,e                 
        ld       (hl),b              
        ld       l,a                 
        ld       l,c                 
        ld       l,(hl)              
        ld       (hl),h              
        jr       nz,$+42             
        jr       nc,$+50             
        jr       nc,$+50             
        jr       nz,$+101            
        ld       l,h                 
        ld       h,l                 
        ld       h,c                 
        ld       (hl),d              
        ld       (hl),e              
        add      hl,hl               
        nop                          
        ld       b,h                 
        ld       b,d                 
        ld       a,h                 
        ld       b,h                 
        ld       l,c                 
        ld       (hl),e              
        ld       (hl),b              
        ld       l,h                 
        ld       h,c                 
        ld       a,c                 
        jr       nz,$+100            
        ld       (hl),d              
        ld       h,l                 
        ld       h,c                 
        ld       l,e                 
        ld       (hl),b              
        ld       l,a                 
        ld       l,c                 
        ld       l,(hl)              
        ld       (hl),h              
        ld       (hl),e              
        nop                          
        ld       b,h                 
        ld       c,c                 
        jr       nz,$+104            
        ld       (hl),d              
        ld       l,a                 
        ld       l,l                 
        inc      l                   
        ld       e,e                 
        ld       (hl),h              
        ld       l,a                 
        ld       e,l                 
        ld       a,h                 
        ld       b,h                 
        ld       l,c                 
        ld       (hl),e              
        ld       h,c                 
        ld       (hl),e              
        ld       (hl),e              
        ld       h,l                 
        ld       l,l                 
        ld       h,d                 
        ld       l,h                 
        ld       h,l                 
        jr       nz,$+111            
        ld       h,l                 
        ld       l,l                 
        ld       l,a                 
        ld       (hl),d              
        ld       a,c                 
        nop                          
        ld       b,h                 
        ld       c,l                 
        jr       nz,$+104            
        ld       (hl),d              
        ld       l,a                 
        ld       l,l                 
        inc      l                   
        ld       e,e                 
        ld       (hl),h              
        ld       l,a                 
        ld       e,l                 
        ld       a,h                 
        ld       b,h                 
        ld       (hl),l              
        ld       l,l                 
        ld       (hl),b              
        jr       nz,$+111            
        ld       h,l                 
        ld       l,l                 
        ld       l,a                 
        ld       (hl),d              
        ld       a,c                 
        jr       nz,$+42             
        ld       c,b                 
        ld       b,l                 
        ld       e,b                 
        cpl                          
        ld       b,c                 
        ld       d,e                 
        ld       b,e                 
        ld       c,c                 
        ld       c,c                 
        add      hl,hl               
        nop                          
        ld       b,h                 
        ld       d,d                 
        ld       a,h                 
        ld       b,h                 
        ld       l,c                 
        ld       (hl),e              
        ld       (hl),b              
        ld       l,h                 
        ld       h,c                 
        ld       a,c                 
        jr       nz,$+92             
        jr       c,$+50              
        jr       nz,$+116            
        ld       h,l                 
        ld       h,a                 
        ld       l,c                 
        ld       (hl),e              
        ld       (hl),h              
        ld       h,l                 
        ld       (hl),d              
        ld       (hl),e              
        nop                          
        ld       b,l                 
        jr       nz,$+99             
        ld       h,h                 
        ld       h,h                 
        ld       (hl),d              
        ld       a,h                 
        ld       b,l                 
        ld       h,h                 
        ld       l,c                 
        ld       (hl),h              
        jr       nz,$+111            
        ld       h,l                 
        ld       l,l                 
        ld       l,a                 
        ld       (hl),d              
        ld       a,c                 
        nop                          
        ld       b,(hl)              
        jr       nz,$+104            
        ld       (hl),d              
        ld       l,a                 
        ld       l,l                 
        inc      l                   
        ld       (hl),h              
        ld       l,a                 
        jr       nz,$+120            
        ld       h,c                 
        ld       l,h                 
        ld       (hl),l              
        ld       h,l                 
        ld       a,h                 
        ld       b,(hl)              
        ld       l,c                 
        ld       l,h                 
        ld       l,h                 
        jr       nz,$+111            
        ld       h,l                 
        ld       l,l                 
        ld       l,a                 
        ld       (hl),d              
        ld       a,c                 
        nop                          
        ld       b,a                 
        jr       nz,$+93             
        ld       h,c                 
        ld       h,h                 
        ld       h,h                 
        ld       (hl),d              
        ld       e,l                 
        ld       a,h                 
        ld       b,a                 
        ld       l,a                 
        jr       nz,$+42             
        ld       h,l                 
        ld       a,b                 
        ld       h,l                 
        ld       h,e                 
        ld       (hl),l              
        ld       (hl),h              
        ld       h,l                 
        add      hl,hl               
        nop                          
        ld       c,c                 
        jr       nz,$+114            
        ld       l,a                 
        ld       (hl),d              
        ld       (hl),h              
        ld       a,h                 
        ld       d,d                 
        ld       h,l                 
        ld       h,c                 
        ld       h,h                 
        cpl                          
        ld       b,h                 
        ld       l,c                 
        ld       (hl),e              
        ld       (hl),b              
        ld       l,h                 
        ld       h,c                 
        ld       a,c                 
        jr       nz,$+75             
        cpl                          
        ld       c,a                 
        jr       nz,$+114            
        ld       l,a                 
        ld       (hl),d              
        ld       (hl),h              
        nop                          
        ld       c,h                 
        ld       a,h                 
        ld       c,h                 
        ld       l,a                 
        ld       h,c                 
        ld       h,h                 
        jr       nz,$+48             
        ld       c,b                 
        ld       b,l                 
        ld       e,b                 
        jr       nz,$+104            
        ld       l,c                 
        ld       l,h                 
        ld       h,l                 
        nop                          
        ld       c,a                 
        jr       nz,$+114            
        ld       l,a                 
        ld       (hl),d              
        ld       (hl),h              
        jr       nz,$+120            
        ld       h,c                 
        ld       l,h                 
        ld       (hl),l              
        ld       h,l                 
        ld       a,h                 
        ld       d,a                 
        ld       (hl),d              
        ld       l,c                 
        ld       (hl),h              
        ld       h,l                 
        jr       nz,$+75             
        cpl                          
        ld       c,a                 
        jr       nz,$+114            
        ld       l,a                 
        ld       (hl),d              
        ld       (hl),h              
        nop                          
        ld       d,h                 
        ld       a,h                 
        ld       d,h                 
        ld       (hl),d              
        ld       h,c                 
        ld       h,e                 
        ld       h,l                 
        jr       nz,$+42             
        ld       (hl),e              
        ld       l,c                 
        ld       l,(hl)              
        ld       h,a                 
        ld       l,h                 
        ld       h,l                 
        dec      l                   
        ld       (hl),e              
        ld       (hl),h              
        ld       h,l                 
        ld       (hl),b              
        add      hl,hl               
        nop                          
        ld       b,c                 
        ld       b,(hl)              
        inc      l                   
        ld       b,d                 
        ld       b,e                 
        inc      l                   
        ld       b,h                 
        ld       b,l                 
        inc      l                   
        ld       c,b                 
        ld       c,h                 
        nop                          
        ld       c,c                 
        ld       e,b                 
        inc      l                   
        ld       c,c                 
        ld       e,c                 
        inc      l                   
        ld       d,e                 
        ld       d,b                 
        inc      l                   
        ld       d,b                 
        ld       b,e                 
        jr       nz,$+120            
        ld       h,c                 
        ld       l,h                 
        ld       (hl),l              
        ld       h,l                 
        ld       a,h                 
        ld       d,e                 
        ld       h,l                 
        ld       (hl),h              
        jr       nz,$+116            
        ld       h,l                 
        ld       h,a                 
        ld       l,c                 
        ld       (hl),e              
        ld       (hl),h              
        ld       h,l                 
        ld       (hl),d              
        jr       nz,$+120            
        ld       h,c                 
        ld       l,h                 
        ld       (hl),l              
        ld       h,l                 
        nop                          
        nop                          
        ld       a,003h              
        out      (080h),a            
        out      (080h),a            
        ld       a,077h              
        out      (080h),a            
        ld       a,015h              
        out      (080h),a            
        ret                          
        in       a,(080h)            
        and      001h                
        ret      z                   
        in       a,(081h)            
        ret                          
        push     af                  
        in       a,(080h)            
        and      002h                
        jr       z,$-4               
        pop      af                  
        out      (081h),a            
        ret                          
