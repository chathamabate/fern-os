
;; Copy a string into a buffer.
;; NOTE: This does not copy
;; 
;; Frame:
;;  [bp-1]: u16 character written (Return)
;;  [bp-3]: u16 src segment.
;;  [bp-5]: u16 src offset.
;;  [bp-7]: u16 dest segment.
;;  [bp-9]: u16 dest offset.
;;  [bp-11]: u16 dest size.
str_copy:
    mov ax, [bp-3]
    mov ds, ax
    mov si, [bp-5]

    mov ax, [bp-7]
    mov es, ax
    mov di, [bp-9]

    ;; Copy from ds:si to es:di

    
