augroup highlight_f2
  autocmd!
  autocmd FileType c syntax match HighlightF2 /\<f2\>/
  autocmd FileType c highlight link HighlightF2 cType
augroup END
