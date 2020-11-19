import sys
import fileinput
import re

for line in fileinput.input(sys.argv[1], inplace=1):
	if re.match("main:", line) is not None:
		print(line, end="")
		print("\tCALLA #init ; init")
		#print("\tCALLA #reset_event_list")
		print("; inserting stack protection")
		print(".LPROT:")
		print("\tMOV &0x4000, R12")
		# Assumption: R12 is not populated at this moment
		print("\tCMP.W #-1, R12 { JEQ\t.LPROTEND ; check prev checkpoint") 
		# Restore
		print("\tCALLA #restore")
		print(".LPROTEND:")
		# only on first boot, manually set SP to NV stack. Other times 
		# restore routine sets the SP
		print("\tMOV #20540, R1 ; NV stack protection")
	else:
		print(line, end="")
