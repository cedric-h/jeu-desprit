autocmd BufRead,BufNewFile *.h set filetype=c
augroup highlight_f2
  autocmd!
  autocmd FileType c syntax match HighlightF2 /\<f2\>/
  autocmd FileType c highlight link HighlightF2 cType

  autocmd FileType c syntax match HighlightF3 /\<f3\>/
  autocmd FileType c highlight link HighlightF3 cType

  autocmd FileType c syntax match HighlightF4 /\<f4\>/
  autocmd FileType c highlight link HighlightF4 cType

  autocmd FileType c syntax match HighlightF4x4 /\<f4x4\>/
  autocmd FileType c highlight link HighlightF4x4 cType
augroup END
