find ./ -type f |grep -v svn |awk '{print "sed -i '\''s/list_add_head_tail/list_add_tail/g'\'' " $0}'  |sh
